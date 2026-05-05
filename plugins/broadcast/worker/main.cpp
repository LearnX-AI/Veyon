/*
 * veyon-broadcast-worker - renders a broadcast overlay in the user session
 *
 * Spawned by BroadcastPlugin (server side) once per message.
 * Receives message data via command-line args.
 *
 * Args (all required, all base64 to avoid quoting issues):
 *   --level     Normal|Urgent|Emergency
 *   --title     base64-encoded UTF-8 title (may be empty)
 *   --body      base64-encoded UTF-8 body
 *
 * Exit conditions:
 *   - Normal:    auto-close after 10s, or user clicks close (X)
 *   - Urgent:    user clicks Acknowledge button
 *   - Emergency: process killed by parent (teacher dismisses centrally)
 */

#include <QApplication>
#include <QByteArray>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>


enum class Level { Normal, Urgent, Emergency };


static Level levelFromString( const QString& s )
{
    if( s == QStringLiteral("Urgent") )    return Level::Urgent;
    if( s == QStringLiteral("Emergency") ) return Level::Emergency;
    return Level::Normal;
}


// Emergency overlay swallows Esc/Alt-F4 so students cannot dismiss it.
class EmergencyOverlay : public QWidget
{
public:
    EmergencyOverlay() : QWidget( nullptr )
    {
        setWindowFlags( Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool );
        setAttribute( Qt::WA_ShowWithoutActivating, false );
    }
protected:
    void keyPressEvent( QKeyEvent* e ) override
    {
        // Block all keys (Esc, Alt-F4, etc.)
        e->accept();
    }
    void closeEvent( QCloseEvent* e ) override
    {
        e->ignore();   // user cannot close
    }
};


static QWidget* makeNormalPopup( const QString& body )
{
    auto* w = new QWidget( nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint );
    w->setStyleSheet( QStringLiteral(
        "background-color: #2c3e50; color: white; border-radius: 8px; padding: 16px;"
    ) );
    auto* layout = new QVBoxLayout( w );
    auto* label = new QLabel( body );
    label->setWordWrap( true );
    label->setTextFormat( Qt::PlainText );   // SECURITY: never render as HTML
    label->setStyleSheet( QStringLiteral("font-size: 14pt;") );
    layout->addWidget( label );
    w->setFixedSize( 380, 140 );

    // Dock top-right of primary screen
    if( const auto* screen = QGuiApplication::primaryScreen() )
    {
        const QRect g = screen->availableGeometry();
        w->move( g.right() - w->width() - 24, g.top() + 24 );
    }
    return w;
}


static QWidget* makeUrgentOverlay( const QString& body, QApplication& app )
{
    auto* w = new QWidget( nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint );
    w->setStyleSheet( QStringLiteral("background-color: rgba(0,0,0,220);") );
    auto* layout = new QVBoxLayout( w );
    layout->setAlignment( Qt::AlignCenter );

    auto* label = new QLabel( body );
    label->setWordWrap( true );
    label->setTextFormat( Qt::PlainText );
    label->setAlignment( Qt::AlignCenter );
    label->setStyleSheet( QStringLiteral(
        "color: white; font-size: 28pt; padding: 40px;"
    ) );
    layout->addWidget( label );

    auto* ackBtn = new QPushButton( QObject::tr("Acknowledge") );
    ackBtn->setStyleSheet( QStringLiteral(
        "background-color: #27ae60; color: white; padding: 14px 36px;"
        "font-size: 14pt; border: none; border-radius: 6px;"
    ) );
    layout->addWidget( ackBtn, 0, Qt::AlignCenter );
    QObject::connect( ackBtn, &QPushButton::clicked, &app, &QApplication::quit );

    if( const auto* screen = QGuiApplication::primaryScreen() )
    {
        w->setGeometry( screen->geometry() );
    }
    return w;
}


static QWidget* makeEmergencyOverlay( const QString& body )
{
    auto* w = new EmergencyOverlay();
    w->setStyleSheet( QStringLiteral("background-color: #c0392b;") );   // red
    auto* layout = new QVBoxLayout( w );
    layout->setAlignment( Qt::AlignCenter );

    auto* heading = new QLabel( QObject::tr("EMERGENCY") );
    heading->setAlignment( Qt::AlignCenter );
    heading->setStyleSheet( QStringLiteral(
        "color: white; font-size: 56pt; font-weight: bold; padding: 20px;"
    ) );
    layout->addWidget( heading );

    auto* label = new QLabel( body );
    label->setWordWrap( true );
    label->setTextFormat( Qt::PlainText );
    label->setAlignment( Qt::AlignCenter );
    label->setStyleSheet( QStringLiteral(
        "color: white; font-size: 32pt; padding: 40px;"
    ) );
    layout->addWidget( label );

    if( const auto* screen = QGuiApplication::primaryScreen() )
    {
        w->setGeometry( screen->geometry() );
    }
    return w;
}


int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    QCommandLineParser p;
    QCommandLineOption levelOpt( QStringLiteral("level"), QStringLiteral("Normal|Urgent|Emergency"), QStringLiteral("level") );
    QCommandLineOption titleOpt( QStringLiteral("title"), QStringLiteral("base64 title"), QStringLiteral("title") );
    QCommandLineOption bodyOpt(  QStringLiteral("body"),  QStringLiteral("base64 body"),  QStringLiteral("body") );
    p.addOption( levelOpt );
    p.addOption( titleOpt );
    p.addOption( bodyOpt );
    p.process( app );

    const Level level   = levelFromString( p.value( levelOpt ) );
    const QString body  = QString::fromUtf8( QByteArray::fromBase64( p.value( bodyOpt ).toLatin1() ) );

    if( body.trimmed().isEmpty() )
    {
        return 1;   // refuse to show empty overlays
    }

    QWidget* overlay = nullptr;
    switch( level )
    {
        case Level::Normal:    overlay = makeNormalPopup( body );          break;
        case Level::Urgent:    overlay = makeUrgentOverlay( body, app );   break;
        case Level::Emergency: overlay = makeEmergencyOverlay( body );     break;
    }

    overlay->show();

    // Auto-close behavior:
    if( level == Level::Normal )
    {
        QTimer::singleShot( 10000, &app, &QApplication::quit );
    }
    // Urgent: user clicks Acknowledge (handled in makeUrgentOverlay)
    // Emergency: parent process kills us when teacher dismisses

    return app.exec();
}
