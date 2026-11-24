/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <QMouseEvent>
#include <QTimer>
#include <QCursor>
#include <QWidget>
#include "qtcompat.h"

#include "debugging/debugging.h"


// QMouseEvent is not copyable in Qt6; keep the state we need and rebuild the event on demand.
class MouseEventState
{
	QEvent::Type m_type = QEvent::MouseMove;
	QPointF m_localPos;
	QPointF m_globalPos;
	Qt::MouseButton m_button = Qt::NoButton;
	Qt::MouseButtons m_buttons;
	Qt::KeyboardModifiers m_modifiers;
public:
	MouseEventState() = default;
	explicit MouseEventState( const QMouseEvent& event ) :
		m_type( event.type() ),
#if QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 )
		m_localPos( event.position() ),
		m_globalPos( event.globalPosition() ),
#else
		m_localPos( event.localPos() ),
		m_globalPos( event.screenPos() ),
#endif
		m_button( event.button() ),
		m_buttons( event.buttons() ),
		m_modifiers( event.modifiers() ){
	}
	QMouseEvent event() const {
#if QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 )
		return QMouseEvent( m_type, m_localPos, m_globalPos, m_button, m_buttons, m_modifiers );
#else
		return QMouseEvent( m_type, m_localPos, m_localPos, m_globalPos, m_button, m_buttons, m_modifiers );
#endif
	}
};


class DeferredMotion
{
	MouseEventState m_mouseMoveEvent;
	QTimer m_timer;
public:
	template<class Functor>
	DeferredMotion( Functor func )
	{
		m_timer.setSingleShot( true );
		m_timer.callOnTimeout( [this, func](){ func( m_mouseMoveEvent.event() ); } );
	}
	void motion( const QMouseEvent *event ){
		m_mouseMoveEvent = MouseEventState( *event );
		if( !m_timer.isActive() )
			m_timer.start();
	}
};

class DeferredMotion2
{
	MouseEventState m_mouseMoveEvent;
	const std::function<void( const QMouseEvent& )> m_func;
public:
	template<class Functor>
	DeferredMotion2( Functor func ) :
		m_func( func )
	{
	}
	void motion( const QMouseEvent& event ){
		m_mouseMoveEvent = MouseEventState( event );
	}
	void invoke(){
		m_func( m_mouseMoveEvent.event() );
	}
	typedef MemberCaller<DeferredMotion2, void(), &DeferredMotion2::invoke> InvokeCaller;
};

class DeferredMotionDelta
{
	MouseEventState m_mouseMoveEvent;
	QTimer m_timer;
	int m_delta_x = 0;
	int m_delta_y = 0;
public:
	template<class Functor>
	DeferredMotionDelta( Functor func )
	{
		m_timer.setSingleShot( true );
		m_timer.callOnTimeout( [this, func](){
			func( m_delta_x, m_delta_y, m_mouseMoveEvent.event() );
			m_delta_x = 0;
			m_delta_y = 0;
		} );
	}
	void flush(){
		m_timer.stop();
//.	?	deferred_motion( this );
	}
	void motion_delta( int x, int y, const QMouseEvent *event ){
		m_delta_x += x;
		m_delta_y += y;
		m_mouseMoveEvent = MouseEventState( *event );
		if( !m_timer.isActive() )
			m_timer.start();
	}
};

class DeferredMotionDelta2
{
	MouseEventState m_mouseMoveEvent;
	std::function<void( int, int, const QMouseEvent& )> m_func;
	int m_delta_x = 0;
	int m_delta_y = 0;
public:
	template<class Functor>
	DeferredMotionDelta2( Functor func ) :
		m_func( func )
	{
	}
	void motion_delta( int x, int y, const QMouseEvent *event ){
		m_delta_x += x;
		m_delta_y += y;
		m_mouseMoveEvent = MouseEventState( *event );
	}
	void invoke(){
		m_func( m_delta_x, m_delta_y, m_mouseMoveEvent.event() );
		m_delta_x = 0;
		m_delta_y = 0;
	}
	typedef MemberCaller<DeferredMotionDelta2, void(), &DeferredMotionDelta2::invoke> InvokeCaller;
};



class FreezePointer : public QObject
{
	QWidget* m_widget;

	//global coordinates
	QPoint m_initial_pos;
	bool m_trackingEstablished;

	std::function<void(int, int, const QMouseEvent*)> m_motion_delta_function;
	std::function<void()> m_focus_out_function;
	QTimer m_rescueTimer;
	QPoint getCenter() const {
		return m_widget->mapToGlobal( m_widget->rect().center() );
	}
protected:
	bool eventFilter( QObject *obj, QEvent *event ) override {
		if( event->type() == QEvent::MouseMove ) {
			auto *mouseEvent = static_cast<QMouseEvent *>( event );
			const QPoint center = getCenter();
			/* QCursor::setPos( center ) effect may happen not immediately; suspend processing till then, otherwise start dash will happen */
			if( !m_trackingEstablished ){
				m_trackingEstablished = qt_mouse_global_pos( *mouseEvent ) == center;
				QCursor::setPos( center );
			}
			else if( qt_mouse_global_pos( *mouseEvent ) != center ){
				const QPoint delta = qt_mouse_global_pos( *mouseEvent ) - center;
				m_motion_delta_function( delta.x(), delta.y(), mouseEvent );
				QCursor::setPos( center );
			}
			// handle runaways with released buttons; FIXME: need more elegant way to persistently grab in this case
			if( !m_widget->rect().contains( mouseEvent->pos() ) ){ // bomb cursor via timer to get it back to the widget
				if( !m_rescueTimer.isActive() ){
					m_rescueTimer.disconnect(); // disconnect everything
					m_rescueTimer.callOnTimeout( [center = center](){ QCursor::setPos( center ); } );
					m_rescueTimer.start( 33 );
				}
			}
			else{
				m_rescueTimer.stop();
			}
		}
		else if( event->type() == QEvent::FocusOut ) {
			m_focus_out_function();
		}
		return QObject::eventFilter( obj, event ); // standard event processing
	}
public:
	FreezePointer() : m_widget( nullptr ){
	}

	void freeze_pointer( QWidget* widget, decltype( m_motion_delta_function ) motion_delta_function, decltype( m_focus_out_function ) focus_out_function ){
		ASSERT_MESSAGE( m_widget == nullptr, "can't freeze pointer: already frozen" );

		m_widget = widget;
		m_initial_pos = QCursor::pos();
		m_trackingEstablished = false;
		m_motion_delta_function = motion_delta_function;
		m_focus_out_function = focus_out_function;

		m_widget->installEventFilter( this );
		m_widget->grabMouse( Qt::CursorShape::BlankCursor ); // is only needed for released mouse freezing, prevents certain glitches
		/* using center for tracking for max safety */
		QCursor::setPos( getCenter() );
	}

	void unfreeze_pointer( bool centerize ){
		ASSERT_MESSAGE( m_widget != nullptr, "can't unfreeze pointer: is not frozen" );

		m_rescueTimer.stop();
		m_widget->releaseMouse();
		m_widget->removeEventFilter( this );

		if( centerize )
			QCursor::setPos( getCenter() );
		else
			QCursor::setPos( m_initial_pos );

		m_widget = nullptr;
	}
};





class DeferredAdjustment
{
	int m_value;
	QTimer m_timer;
public:
	template<class Functor>
	DeferredAdjustment( Functor func ) {
		m_timer.setSingleShot( true );
		m_timer.callOnTimeout( [this, func](){ func( m_value ); } );
	}
	void value_changed( int value ){
		m_value = value;
		if( !m_timer.isActive() )
			m_timer.start();
	}
};
