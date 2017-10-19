/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#include "gui/MainWindow.h"
#include "schema/ExpressToSourceConverter.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qtabwidget.h>
#include <QtWidgets/QSplashScreen>
#include <QtCore/QTimer>
#include <QtCore/qstringlist.h>

#include <iostream>
#include <sstream>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	Q_INIT_RESOURCE( resources );

	QPixmap pixmap( ":Resources/img/IfcPlusPlusSplashscreen.png" );
	QSplashScreen splash( pixmap );
	splash.show();
	a.processEvents();

	MainWindow w;
	w.show();
	QTimer::singleShot( 1500, &splash, SLOT(close()));
	a.processEvents();

	return a.exec();
}
