/* Arduino SdFat Library
 * Copyright (C) 2015 by William Greiman
 *
 * This file is part of the Arduino SdFat Library
 *
 * This Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with the Arduino SdFat Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef FreeStack_h
#define FreeStack_h
/**
 * \file
 * \brief FreeStack() function.
 */
#ifdef __arm__
extern "C" char* sbrk(int incr);
static int FreeStack() {
  char top;
  return &top - reinterpret_cast<char*>(sbrk(0));
}
#else  // __arm__
/** boundary between stack and heap. */
extern char *__brkval;
/** End of bss section.*/
extern char __bss_end;
/** Amount of free stack space.
 * \return The number of free bytes.
 */
static int FreeStack() {
  char top;
  return __brkval ? &top - __brkval : &top - &__bss_end;
}
#endif  // __arm
#endif  // FreeStack_h
