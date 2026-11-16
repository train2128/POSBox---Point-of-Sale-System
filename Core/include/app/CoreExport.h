#ifndef COREEXPORT_H
#define COREEXPORT_H

#include <QtCore/qglobal.h>

// Mark a Core class CORE_EXPORT when an application connects to its signals or
// slots with the &Class::member syntax. Qt finds a signal by comparing function
// addresses, and on Windows an application sees a different address for an
// unmarked DLL function, so the connect fails at runtime with only a warning.
#if defined(CORE_LIBRARY)
#  define CORE_EXPORT Q_DECL_EXPORT
#else
#  define CORE_EXPORT Q_DECL_IMPORT
#endif

#endif // COREEXPORT_H
