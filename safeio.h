// ==============================================================================
/**
 * safeio.h
 *
 * Safe I/O (well, just O) functions that do not rely on heap allocation.
 **/
// ==============================================================================



// ==============================================================================
// Avoid multiple inclusion.

#if !defined (_SAFEIO_H)
#define _SAFEIO_H
// ==============================================================================



// ==============================================================================
// MACROS

#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))

/** Emit an error message. */
#define ERROR(msg,...) safe_error(msg, NUMARGS(__VA_ARGS__), ##__VA_ARGS__)

/** Emit a debugging message (or, if disabled, remove such output). */
#if defined (DEBUG_ALLOC)
#define DEBUG(msg,...) safe_debug(msg, NUMARGS(__VA_ARGS__), ##__VA_ARGS__)
#else
#define DEBUG(msg,...)
#endif /* DEBUG_ALLOC */
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
void safe_debug (const char* msg, int argc, ...);

/**
 * Print an error message and abort the process.  **Does not return**
 *
 * \param msg The string to emit as a message to `stderr`.  Cannot be longer
 *            than 256 characters.
 * \param argc Count of the variadic arguments.
 * \param ...  The variadic arguments (0 or more) of integers to be appended to
 *             the output.
 */
void safe_error (const char* msg, int argc, ...);
// ==============================================================================



// ==============================================================================
#endif // _SAEFIO_H
// ==============================================================================
