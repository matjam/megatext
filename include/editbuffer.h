#ifndef __EDITBUFFER_H__
#define __EDITBUFFER_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"

// Edit buffer
//
// These routines provide an edit buffer for performing text manipulation. The buffer is
// dynamically allocated and can grow as needed. The buffer is a linked list of string fragments
// that are concatenated together to form the complete buffer.
//
// Characters can be inserted, deleted, and replaced in the buffer in any order, at any position.
//
// A "view" into the buffer is maintained that represents the current screen display. The view
// is a pointer to a position in the buffer, and a length that represents the number of characters
// to display. The view can be scrolled up or down to display different parts of the buffer.
//
// When the buffer is first loaded with text, the view is positioned at the beginning of the buffer,
// and the buffer is scrolled to the top. Lines are wrapped to the next line when they exceed the
// width of the screen which is a fixed 80 characters.
//
// Each line represented in the buffer also maintains state on the position of the wrap, and the
// position of the cursor. The cursor is a pointer to a position in the line, and is used to
// determine where to insert, delete, or replace characters in the line.

typedef struct editbuffer editbuffer_t;

// Create a new edit buffer
editbuffer_t __huge* editbuffer_create();

// Destroy an edit buffer
void editbuffer_destroy(editbuffer_t __huge* eb);

// Load text into the edit buffer
void editbuffer_load(editbuffer_t __huge* eb, const char* text);

// Get the wrapped text from the edit buffer
char __huge* editbuffer_get_text(editbuffer_t __huge* eb);

// append a character to the edit buffer
void editbuffer_append(editbuffer_t __huge* eb, char ch);

// insert a character at the current cursor position
void editbuffer_insert(editbuffer_t __huge* eb, char ch);

// delete the character at the current cursor position
void editbuffer_delete(editbuffer_t __huge* eb);

// replace the character at the current cursor position
void editbuffer_replace(editbuffer_t __huge* eb, char ch);

// move the cursor to the left
void editbuffer_cursor_left(editbuffer_t __huge* eb);

// move the cursor to the right
void editbuffer_cursor_right(editbuffer_t __huge* eb);

#endif