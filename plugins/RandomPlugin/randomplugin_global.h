#ifndef RANDOMPLUGIN_GLOBAL_H
#define RANDOMPLUGIN_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(RANDOMPLUGIN_LIBRARY)
#  define RANDOMPLUGINSHARED_EXPORT Q_DECL_EXPORT
#else
#  define RANDOMPLUGINSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // RANDOMPLUGIN_GLOBAL_H