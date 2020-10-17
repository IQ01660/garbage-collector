// ==============================================================================
/**
 * safeio.c
 *
 * Safe I/O (well, just O) functions that do not rely on heap allocation.
 **/
// ==============================================================================



// ==============================================================================
// INCLUDES

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "safeio.h"
// ==============================================================================



// ==============================================================================
// MACRO CONSTANTS AND FUNCTIONS

#define BITS_PER_BYTE    8
#define BITS_PER_NYBBLE  4
#define BYTE_MASK        0xff
#define NYBBLE_MASK      0xf

#define BYTES_PER_WORD   sizeof(intptr_t)
#define BITS_PER_WORD    (BYTES_PER_WORD * BITS_PER_BYTE)
#define NYBBLES_PER_WORD (BITS_PER_WORD / BITS_PER_NYBBLE)

/** The maximum length of debugging/error messages. */
#define MAX_MESSAGE_LENGTH 256

#define TAB_STRING "\t"
#define TAB_LENGTH 1

#define NEWLINE_STRING "\n"
#define NEWLINE_LENGTH 1

#define OUTPUT_FD  STDERR_FILENO
// ==============================================================================



// ==============================================================================
void
int_to_hex (char* buffer, uint64_t value) {

  static char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7',
			       '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
  char* current  = buffer;
  bool  non_zero = false;
  for (int i = NYBBLES_PER_WORD - 1; i >= 0; i = i - 1) {
    unsigned int nybble = (value >> (i * BITS_PER_NYBBLE)) & NYBBLE_MASK;
    if (non_zero || nybble != 0) {
      non_zero   = true;
      *current++ = hex_digits[nybble];
    }
  }

  if (!non_zero) {
    *current++ = hex_digits[0];
  }

  *current = '\0';
  
} // int_to_hex ()
// ==============================================================================



// ==============================================================================
/**
 * Print a message.
 *
 * \param prefix The string to emit as a prefix.
 * \param msg    The string to emit as a message.
 * \param argc   Count of the variadic arguments.
 * \param argp   The variadic arguments of integers to be appended to the output.
 */
void
emit (const char* prefix, const char* msg, int argc, va_list argp) {
  
  // Emit the prefix and message.
  write(OUTPUT_FD, prefix, strnlen(prefix, MAX_MESSAGE_LENGTH));
  write(OUTPUT_FD, msg,    strnlen(msg,    MAX_MESSAGE_LENGTH));

  // Emit each given integer with a tab prefix.
  for (int i = 0; i < argc; ++i) {
    uint64_t value = va_arg(argp, uint64_t);
    char     buffer[MAX_MESSAGE_LENGTH];
    int_to_hex(buffer, value);
    write(OUTPUT_FD, TAB_STRING, TAB_LENGTH);
    write(OUTPUT_FD, buffer,     strnlen(buffer, MAX_MESSAGE_LENGTH));
  }

  // Emit a newline and flush the output.
  write(OUTPUT_FD, NEWLINE_STRING, NEWLINE_LENGTH);
  fsync(OUTPUT_FD);

}
// ==============================================================================



// ==============================================================================
/**
 * Print an debugging message.
 *
 * \param msg  The string to emit as a message to `stderr`.  Cannot be longer
 *             than 256 characters.
 * \param argc Count of the variadic arguments.
 * \param ...  The variadic arguments (0 or more) of integers to be appended to
 *             the output.
 */
void
safe_debug (const char* msg, int argc, ...) {

  // Emit the debugging message.
  va_list argp;
  va_start(argp, argc);
  emit("DEBUG: ", msg, argc, argp);
  va_end(argp);
  
} // safe_debug ()
// ==============================================================================



// ==============================================================================
/**
 * Print an error message and abort the process.  **Does not return**
 *
 * \param msg The string to emit as a message to `stderr`.  Cannot be longer
 *            than 256 characters.
 * \param argc Count of the variadic arguments.
 * \param ...  The variadic arguments (0 or more) of integers to be appended to
 *             the output.
 */
void
safe_error (const char* msg, int argc, ...) {

  // Emit the error message.
  va_list argp;
  va_start(argp, argc);
  emit("ERROR: ", msg, argc, argp);
  va_end(argp);

  // And exit with an error code.
  exit(1);
  
} // safe_error ()
// ==============================================================================
