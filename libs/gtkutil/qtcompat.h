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

// Small wrappers around Qt APIs that were deprecated in Qt 6, so the same sources
// build with both Qt 5 and Qt 6 without deprecation warnings.

#include <QtGlobal>
#include <QPoint>
#include <QKeySequence>
#include <QRegularExpression>
#include <QCheckBox>
#include <QMouseEvent>
#include <QDropEvent>

/// Global (screen) position of a mouse event.
inline QPoint qt_mouse_global_pos( const QMouseEvent& event ) {
#if QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 )
	return event.globalPosition().toPoint();
#else
	return event.globalPos();
#endif
}

/// Position of a drag/drop event.
inline QPoint qt_drop_pos( const QDropEvent& event ) {
#if QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 )
	return event.position().toPoint();
#else
	return event.pos();
#endif
}

/// A key sequence built from a key and its modifiers, without the int round-trip Qt6 deprecated.
inline QKeySequence qt_key_sequence( Qt::KeyboardModifiers modifiers, Qt::Key key ) {
#if QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 )
	return QKeySequence( QKeyCombination( modifiers, key ) );
#else
	return QKeySequence( static_cast<int>( modifiers ) + static_cast<int>( key ) );
#endif
}

/// The given sequence with the keypad modifier added to its first key.
inline QKeySequence qt_key_sequence_with_keypad( const QKeySequence& sequence ) {
#if QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 )
	const QKeyCombination combination = sequence[0];
	return QKeySequence( QKeyCombination( combination.keyboardModifiers() | Qt::KeypadModifier, combination.key() ) );
#else
	return QKeySequence( sequence[0] | Qt::KeypadModifier );
#endif
}

/// Match \p subject at \p offset, anchored to that offset.
inline QRegularExpressionMatch qt_regex_match_anchored( const QRegularExpression& expression, QStringView subject, qsizetype offset ) {
#if QT_VERSION >= QT_VERSION_CHECK( 6, 8, 0 )
	return expression.matchView( subject, offset, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption );
#elif QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 )
	return expression.match( subject, offset, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption );
#else
	return expression.match( subject, offset, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption );
#endif
}

/// Connect a check box state change, using the signal that is current for the Qt version.
template<typename Functor>
inline void qt_checkbox_state_changed( QCheckBox* checkbox, Functor func ) {
#if QT_VERSION >= QT_VERSION_CHECK( 6, 7, 0 )
	QObject::connect( checkbox, &QCheckBox::checkStateChanged, checkbox,
	                  [func]( Qt::CheckState state ){ func( static_cast<int>( state ) ); } );
#else
	QObject::connect( checkbox, &QCheckBox::stateChanged, checkbox, func );
#endif
}
