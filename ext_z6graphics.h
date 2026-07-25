! ext_z6graphics.h
! ----------------------------------------------------------------------------
! Z-machine version 6 support for PunyInform: windows, cursor, colour, fonts,
! text measuring, the mouse, and pictures.
!
! PunyInform targets z3/z4/z5/z8 and knows nothing about v6's windowed screen
! model. This extension wraps the v6 opcodes in named routines so a game can use
! them without dropping into assembler.
!
! v6 measures the screen in "units", and what a unit is depends on the
! interpreter. None of them is a text cell you can count on:
!   Ozmoo   reports the 320x200 space Infocom drew the art in -- font 4x8 on the
!           80-column graphics screens, 8x8 on the 40-column one, picture_data in
!           native art pixels. (Ozmoo counted whole cells, font 1x1, until 2026;
!           its -pu:0 switch still does, but that is not the default.)
!   sfrotz  reports 640x400, font 16x8, picture_data doubled -- twice Ozmoo's on
!           both axes.
! Every opcode here uses the same unit as every other within one interpreter, so
! feeding the result of one straight into another (e.g. Z6CursorRow into
! Z6DrawPicture) is always correct without the game knowing which is in play.
! What is NOT safe is mixing a unit with a text row or column count: that used to
! work on Ozmoo by luck, when a unit happened to be a cell, and it does not any
! more. Convert through the font size (property 13) at the boundary -- the Cell
! helpers below do this -- and the game is right on every interpreter.
! ----------------------------------------------------------------------------

System_file;

#Iffalse (#version_number == 6);
error "ext_z6graphics can only be used when compiling with -v6"
#Endif;

Constant Z6_GRAPHICS;

! ---- window attributes, property 14 / window_style flags (spec 8.8.3.2.5) ----
Constant WIN_WRAPPING    = 1;
Constant WIN_SCROLLING   = 2;
Constant WIN_TRANSCRIPT  = 4;
Constant WIN_BUFFERED    = 8;

! ---- window_style operations ----
Constant WSTYLE_SET      = 0;   ! replace the attributes with the flags given
Constant WSTYLE_ON       = 1;   ! turn the given bits on
Constant WSTYLE_OFF      = 2;   ! turn the given bits off
Constant WSTYLE_TOGGLE   = 3;   ! reverse the given bits
! NB a missing operation operand means WSTYLE_SET, and the interpreter's operand
! array keeps stale values between instructions -- always pass the operation.

! ---- window properties, for Z6GetWindowProp / Z6SetWindowProp (spec 8.8.3) ----
Constant WPROP_Y_COORD      = 0;
Constant WPROP_X_COORD      = 1;
Constant WPROP_Y_SIZE       = 2;
Constant WPROP_X_SIZE       = 3;
Constant WPROP_Y_CURSOR     = 4;
Constant WPROP_X_CURSOR     = 5;
Constant WPROP_LEFT_MARGIN  = 6;
Constant WPROP_RIGHT_MARGIN = 7;
Constant WPROP_NL_ROUTINE   = 8;   ! newline interrupt routine (a packed address)
Constant WPROP_NL_COUNT     = 9;   ! lines until that routine fires
Constant WPROP_TEXT_STYLE   = 10;
Constant WPROP_COLOUR       = 11;  ! background in the high byte, foreground in the low
Constant WPROP_FONT         = 12;
Constant WPROP_FONT_SIZE    = 13;  ! height in the high byte, width in the low
Constant WPROP_ATTRIBUTES   = 14;
Constant WPROP_LINE_COUNT   = 15;  ! -999 postpone [More], 999 never

Constant Z6_CURRENT_WINDOW  = (-3);  ! "the window the game is printing to"

Array _z6g_info --> 2;    ! @picture_data:  word 0 height, word 1 width
Array _z6g_cur  --> 2;    ! @get_cursor:    word 0 row,    word 1 column
Array _z6g_mouse --> 4;   ! @read_mouse:    y, x, button bits, menu word
Array _z6g_s3   --> 64;   ! scratch table for measuring text through stream 3

! ============================ windows =====================================

[ Z6SetWindow p_win;            @set_window p_win; ];
[ Z6SplitWindow p_lines;        @split_window p_lines; ];
[ Z6EraseWindow p_win;          @erase_window p_win; ];
[ Z6EraseLine;                  @erase_line 1; ];
[ Z6ScrollWindow p_win p_lines; @scroll_window p_win p_lines; ]; ! negative scrolls back
[ Z6BufferMode p_flag;          @buffer_mode p_flag; ];

[ Z6MoveWindow p_win p_y p_x;   @move_window p_win p_y p_x; ];
[ Z6SizeWindow p_win p_h p_w;   @window_size p_win p_h p_w; ];

! Place a window, size it, blank it and start printing to it.
[ Z6ShowWindow p_win p_y p_x p_h p_w;
    @move_window p_win p_y p_x;
    @window_size p_win p_h p_w;
    @erase_window p_win;
    @set_window p_win;
];

! Blank a window and collapse it, then go back to the main window.
! NB v6 windows are not layered: erasing one leaves a blank hole rather than
! revealing whatever window 0 had underneath, so the game must redraw that area
! itself if it mattered.
[ Z6HideWindow p_win;
    @erase_window p_win;
    @window_size p_win 0 0;
    @set_window 0;
];

! Set/clear a window's wrapping, scrolling, transcript and buffering bits, e.g.
!   Z6FormatWindow(1, WIN_WRAPPING + WIN_SCROLLING, WSTYLE_ON);
[ Z6FormatWindow p_win p_flags p_op;
    @window_style p_win p_flags p_op;
];

[ Z6GetWindowProp p_win p_prop _v;
    @get_wind_prop p_win p_prop -> _v;
    return _v;
];
[ Z6SetWindowProp p_win p_prop p_val;
    @put_wind_prop p_win p_prop p_val;
];

! ============================ units ========================================
! Property 13 is the font size: height in the high byte, width in the low. It
! is the conversion factor between TEXT ROWS/COLUMNS and the interpreter's
! units, and the only portable way to ask which is which. Ozmoo answers 8x4 on
! its 80-column graphics screen (8x8 at 40 columns), sfrotz answers 16x8; the
! long-gone 1x1 (a unit is a cell) only comes back under Ozmoo's -pu:0.
!
! So: units = cells * font size. Anything that mixes a row/column count with a
! coordinate must go through Z6RowsToY / Z6ColsToX, or it will be several times
! wrong -- which is a picture drawn off the bottom of the screen, not an
! obviously wrong number. This bites Ozmoo now too, not only sfrotz.

[ Z6FontHeight _v;
    _v = (Z6GetWindowProp(Z6_CURRENT_WINDOW, WPROP_FONT_SIZE) & $ff00) / $100;
    if(_v < 1) _v = 1;      ! never divide by zero; Arthur is bitten by this too
    return _v;
];
[ Z6FontWidth _v;
    _v = Z6GetWindowProp(Z6_CURRENT_WINDOW, WPROP_FONT_SIZE) & $ff;
    if(_v < 1) _v = 1;
    return _v;
];

[ Z6RowsToY p_rows; return p_rows * Z6FontHeight(); ];
[ Z6ColsToX  p_cols; return p_cols * Z6FontWidth(); ];
[ Z6YToRows p_y;     return p_y / Z6FontHeight(); ];
[ Z6XToCols p_x;     return p_x / Z6FontWidth(); ];

! ============================ cursor and margins ===========================

[ Z6MoveCursor p_y p_x;   @set_cursor p_y p_x; ];

! Move the cursor by TEXT ROW and COLUMN, both 1-based, on any interpreter.
! This is what a game normally wants; Z6MoveCursor's raw units are for code
! that already holds a coordinate it got back from the interpreter.
[ Z6MoveCursorCell p_row p_col _y _x;
    _y = Z6RowsToY(p_row - 1) + 1;
    _x = Z6ColsToX(p_col - 1) + 1;
    @set_cursor _y _x;
];
[ Z6CursorRow;            @get_cursor _z6g_cur; return _z6g_cur-->0; ];
[ Z6CursorCol;            @get_cursor _z6g_cur; return _z6g_cur-->1; ];

! The cursor as a TEXT ROW / COLUMN, both 1-based -- the inverse of
! Z6MoveCursorCell, and what to use whenever a cursor position is about to be
! compared with, or added to, a count of rows (a picture's height, say). The
! raw Z6CursorRow is in units, so `Z6CursorRow() - Z6PictureRows(n)` mixes units
! with rows and is silently several times wrong -- on Ozmoo as much as sfrotz
! now, since Ozmoo's unit is no longer a row. It used to look perfect on Ozmoo,
! which is exactly what made the bug easy to ship.
[ Z6CursorRowCell; return Z6YToRows(Z6CursorRow() - 1) + 1; ];
[ Z6CursorColCell; return Z6XToCols(Z6CursorCol() - 1) + 1; ];
[ Z6HideCursor;           @set_cursor (-1) 0; ];   ! spec 8.7.2.3
[ Z6ShowCursor;           @set_cursor (-2) 0; ];
[ Z6SetMargins p_left p_right p_win; @set_margins p_left p_right p_win; ];

! ============================ colour, style, font ==========================

[ Z6SetColour p_fg p_bg p_win; @set_colour p_fg p_bg p_win; ];
[ Z6SetTextStyle p_style;      @set_text_style p_style; ];

! Returns the previous font, or 0 if the font is not available -- which is how a
! game should test for font 3, the character-graphics font Journey draws its
! menu boxes with.
[ Z6SetFont p_font _prev;
    @set_font p_font -> _prev;
    return _prev;
];

! ============================ measuring text ===============================
! v6 games size their layout by printing to output stream 3 and reading the
! width back from header word $30 (spec 7.1.2.1.1).
! Usage: Z6MeasureBegin(); print "some text"; w = Z6MeasureEnd();
! The width comes back in the interpreter's own units.

[ Z6MeasureBegin; @output_stream 3 _z6g_s3; ];
[ Z6MeasureEnd;   @output_stream (-3); return $30-->0; ];

! Print a table laid out by output_stream 3's formatted (three-operand) form.
[ Z6PrintForm p_table; @print_form p_table; ];

! ============================ mouse ========================================
! Reads into _z6g_mouse: y, x, button bits, menu word. The game must have asked
! for the mouse in Flags 2 bit 5 for the interpreter to track it.

[ Z6ReadMouse;         @read_mouse _z6g_mouse; ];
[ Z6MouseRow;          return _z6g_mouse-->0; ];
[ Z6MouseCol;          return _z6g_mouse-->1; ];
[ Z6MouseButtons;      return _z6g_mouse-->2; ];
[ Z6MouseWindow p_win; @mouse_window p_win; ];   ! confine the pointer to a window

! ============================ pictures =====================================

[ Z6PictureExists p_num;
    @picture_data p_num _z6g_info ?_z6g_pe_yes;
    rfalse;
  ._z6g_pe_yes;
    rtrue;
];

! Picture 0 reports how many pictures the interpreter has (it never branches).
[ Z6PictureCount;
    @picture_data 0 _z6g_info ?_z6g_pc_done;
  ._z6g_pc_done;
    return _z6g_info-->0;
];

[ Z6PictureHeight p_num; if(Z6PictureExists(p_num)) return _z6g_info-->0; rfalse; ];
[ Z6PictureWidth  p_num; if(Z6PictureExists(p_num)) return _z6g_info-->1; rfalse; ];

! The picture's size in TEXT ROWS / COLUMNS, whichever unit the interpreter
! reports in. picture_data answers in units, so this is a plain divide by the
! font size -- not a guess at whether the number "looks like" pixels. (It used
! to be `if(_h > 25) _h = _h / 8`, which silently gave the wrong answer for any
! picture under 26 units tall.)
[ Z6PictureRows p_num _h;
    _h = Z6PictureHeight(p_num);
    if(_h == 0) rfalse;
    _h = Z6YToRows(_h);
    if(_h < 1) _h = 1;
    return _h;
];
[ Z6PictureCols p_num _w;
    _w = Z6PictureWidth(p_num);
    if(_w == 0) rfalse;
    _w = Z6XToCols(_w);
    if(_w < 1) _w = 1;
    return _w;
];

[ Z6DrawPicture p_num p_y p_x;  @draw_picture p_num p_y p_x; ];
[ Z6ErasePicture p_num p_y p_x; @erase_picture p_num p_y p_x; ];

! Draw a picture at the cursor without moving it. Always sends coordinates.
[ Z6DrawPictureHere p_num _y _x;
    @get_cursor _z6g_cur;
    _y = _z6g_cur-->0;
    _x = _z6g_cur-->1;
    @draw_picture p_num _y _x;
];

