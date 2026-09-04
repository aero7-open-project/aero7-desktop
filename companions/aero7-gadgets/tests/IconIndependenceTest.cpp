#include <QApplication>
#include <QIcon>
#include <QImage>

int main(int argc,char **argv){QApplication app(argc,argv);const QStringList paths={":/aero7/icons/app/aero7-gadget-gallery.png",":/aero7/icons/toolbar/online.png"};QList<QImage> baseline;QIcon::setThemeName("Aero7-test-theme");for(const QString &path:paths){const QIcon icon(path);if(icon.isNull())return 1;for(int size:{16,22,24,32,48,64,128,256})if(icon.pixmap(size,size).isNull())return 2;baseline<<icon.pixmap(32,32).toImage();}for(const QString &theme:{QStringLiteral("breeze"),QStringLiteral("breeze-dark"),QStringLiteral("missing-aero7-theme")}){QIcon::setThemeName(theme);for(qsizetype i=0;i<paths.size();++i)if(QIcon(paths.at(i)).pixmap(32,32).toImage()!=baseline.at(i))return 3;}return 0;}
