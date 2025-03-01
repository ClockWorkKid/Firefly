/* Arduino SdFat Library
 * Copyright (C) 2012 by William Greiman
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
 *
 * You should have received a copy of the GNU General Public License
 * along with the Arduino SdFat Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef SdFatUtil_h
#define SdFatUtil_h
/**
 * \file
 * \brief Useful utility functions.
 */
#include "SdFat.h"
/** Store and print a string in flash memory.*/
#define PgmPrint(x) SerialPrint_P(PSTR(x))
/** Store and print a string in flash memory followed by a CR/LF.*/
#define PgmPrintln(x) SerialPrintln_P(PSTR(x))

namespace SdFatUtil {
  /** Amount of free RAM
   * \return The number of free bytes.
   */
  int FreeRam();
  /** %Print a string in flash memory.
   *
   * \param[in] pr Print object for output.
   * \param[in] str Pointer to string stored in flash memory.
   */
  void print_P(Print* pr, PGM_P str);
  /** %Print a string in flash memory followed by a CR/LF.
   *
   * \param[in] pr Print object for output.
   * \param[in] str Pointer to string stored in flash memory.
   */
  void println_P(Print* pr, PGM_P str);
  //----------------------------------------------------------------------------
  /** %Print a string in flash memory to Serial.
   *
   * \param[in] str Pointer to string stored in flash memory.
   */
  inline void SerialPrint_P(PGM_P str) {
    print_P(&Serial, str);
  }
  //----------------------------------------------------------------------------
  /** %Print a string in flash memory to Serial followed by a CR/LF.
   *
   * \param[in] str Pointer to string stored in flash memory.
   */
  inline void SerialPrintln_P(PGM_P str) {
    println_P(&Serial, str);
  }
}  // namespace SdFatUtil
using namespace SdFatUtil;  // NOLINT
#endif  // #define SdFatUtil_h
