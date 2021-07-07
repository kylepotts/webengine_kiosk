#include "Kiosk.h"
#include "KioskView.h"
#include "KioskProgress.h"
#include "ElixirComs.h"
#include "KioskSounds.h"
#include "StderrPipe.h"

#include <QNetworkProxy>
#include <QWebEngineSettings>
#include <QApplication>
#include <QLabel>
#include <QMetaObject>
#include <QMessageBox>

#include <QtQml>
#include <QtQml/QQmlProperty>
#include <QtWebEngineWidgets/QWebEngineView>

Kiosk::Kiosk(const KioskSettings *settings, QObject *parent) :
    QObject(parent),
    settings_(settings),
    coms_(nullptr),
    view_(nullptr),
    loadingPage_(false),
    showPageWhenDone_(true),
    theGoodWindow_(nullptr)
{
    // Set up the UI
    // window_ = new KioskWindow(this, settings);
    // connect(window_, SIGNAL(wakeup()), SLOT(handleWakeup()));

    // window_->setGeometry(calculateWindowRect());

    player_ = settings->soundsEnabled ? new KioskSounds(this) : nullptr;

    qApp->installEventFilter(this);
}

void Kiosk::init()
{
    if (settings_->proxyEnabled) {
        if (settings_->proxySystem) {
            QNetworkProxyFactory::setUseSystemConfiguration(true);
        } else {
            QNetworkProxy proxy;
            proxy.setType(QNetworkProxy::HttpProxy);
            proxy.setHostName(settings_->proxyHostname);
            proxy.setPort(settings_->proxyPort);
            if (!settings_->proxyUsername.isEmpty()) {
                proxy.setUser(settings_->proxyUsername);
                proxy.setPassword(settings_->proxyPassword);
            }
            QNetworkProxy::setApplicationProxy(proxy);
        }
    }

    if (settings_->hideCursor)
        QApplication::setOverrideCursor(Qt::BlankCursor);

    // Set up communication with Elixir
    coms_ = new ElixirComs(this);
    connect(coms_, SIGNAL(messageReceived(KioskMessage)), SLOT(handleRequest(KioskMessage)));

    // Take over stderr
    stderrPipe_ = new StderrPipe(this);
    connect(stderrPipe_, SIGNAL(inputReceived(QByteArray)), SLOT(handleStderr(QByteArray)));

}

void Kiosk::setView(QQuickItem *exview) {
    // Start the browser up
    view_ = exview;
    qDebug() << "SET VIEW: " << view_ << " with: " << exview << "\n";
    // view_->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, settings_->javascriptEnabled);
    // view_->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, settings_->javascriptCanOpenWindows);

    // Set elixir channel
    // elixirChannel_ = new ElixirJsChannel();
    // webChannel_ = new QWebChannel(this);
    // webChannel_->registerObject("elixirJsChannel", elixirChannel_);
    // view_->page()->setWebChannel(webChannel_);

    // connect(view_, SIGNAL(loadStarted()), SLOT(startLoading()));
    // connect(view_, SIGNAL(urlChanged(const QUrl &)), SLOT(urlChanged(const QUrl &)));
    // connect(view_, SIGNAL(loadProgress(int)), SLOT(setProgress(int)));
    // connect(view_, SIGNAL(loadFinished(bool)), SLOT(finishLoading()));    
    // connect(view_, SIGNAL(renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus,int)), SLOT(handleRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus,int)));

    // connect(elixirChannel_, SIGNAL(received(const QString &)), SLOT(elixirMessageReceived(const QString &)));

    goToUrl(settings_->homepage);
}

#include <iostream>     // std::cin, std::cout

void Kiosk::goToUrl(const QUrl &url)
{
    QQmlProperty::write(view_, "url", url);
}

void Kiosk::runJavascript(const QString &program)
{
    QMetaObject::invokeMethod(view_, "runJavaScript", Q_ARG(QString, program));
}

void Kiosk::reload()
{
    QMetaObject::invokeMethod(view_, "reload");
}

void Kiosk::goBack()
{
    QMetaObject::invokeMethod(view_, "goBack");
}

void Kiosk::goForward()
{
    QMetaObject::invokeMethod(view_, "goForward");
}

void Kiosk::stopLoading()
{
    QMetaObject::invokeMethod(view_, "stop");
}

void Kiosk::handleRequest(const KioskMessage &message)
{
    qDebug() << "HANDLING REQUEST" << "\n";
    
    switch (message.type()) {
    case KioskMessage::GoToURL:
        goToUrl(QUrl(QString::fromUtf8(message.payload())));
        break;

    case KioskMessage::RunJavascript:
        runJavascript(QString::fromUtf8(message.payload()));
        break;

    case KioskMessage::Blank:
        // window_->setBrowserVisible(message.payload().at(0) == 0);
        break;

    case KioskMessage::Reload:
        reload();
        break;

    case KioskMessage::GoBack:
        goBack();
        break;

    case KioskMessage::GoForward:
        goForward();
        break;

    case KioskMessage::StopLoading:
        stopLoading();
        break;

    case KioskMessage::SetZoom:
    {
        qreal zoom = message.payload().toDouble();
        if (zoom <= 0.01)
            zoom = 0.01;
        else if (zoom > 10.0)
            zoom = 10.0;

        // view_->page()->setZoomFactor(zoom);
        break;
    }

    default:
        qFatal("Unknown message from Elixir: %d", message.type());
    }
}

static bool isInputEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::TabletPress:
    case QEvent::TabletRelease:
    case QEvent::TabletMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
    case QEvent::ContextMenu:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::Wheel:
        return true;
    default:
        return false;
    }
}

bool Kiosk::eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object);
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (player_)
            player_->play(settings_->windowClickedSound);
        break;

    default:
        break;
    }

    return false;
}

void Kiosk::startLoading()
{
    // if (settings_->progress)
        // window_->showProgress(0);

    coms_->send(KioskMessage::loadingPageMessage());
    loadingPage_ = true;
}

void Kiosk::setProgress(int p)
{
    // if (settings_->progress)
        // window_->showProgress(p);

    coms_->send(KioskMessage::progressMessage(p));

    if (loadingPage_ && p >= 100)
        finishLoading();
}

void Kiosk::finishLoading()
{
    // if (settings_->progress)
        // window_->hideProgress();

    if (loadingPage_) {
        coms_->send(KioskMessage::finishedLoadingPageMessage());
        loadingPage_ = false;

        if (showPageWhenDone_) {
            // Let the event loop settle before showing the browser
            // QTimer::singleShot(100, window_, SLOT(showBrowser()));
        }
    }

    // Force focus just in case it was lost somehow.
    // QApplication::setActiveWindow(window_);
    // window_->focusWidget();

    // Capture the QWidgetWindow reference for the Raspberry Pi
    // input event workaround. See event() function for details.
    // if (!theGoodWindow_) {
    //     // QWidgetWindow is private so verify that it's not a QQuickWindow which
    //     // isn't private and is the only alternative (to my knowledge).
    //     QWindow *win = qApp->focusWindow();
    //     if (dynamic_cast<QQuickWindow *>(win) == nullptr)
    //         theGoodWindow_ = win;
    // }
}

void Kiosk::handleWakeup()
{
    coms_->send(KioskMessage::wakeup());
}

void Kiosk::handleRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus status, int exitCode)
{
    coms_->send(KioskMessage::browserCrashed(status, exitCode));
}

void Kiosk::handleStderr(const QByteArray &line)
{
    coms_->send(KioskMessage::consoleLog(line));
}

void Kiosk::urlChanged(const QUrl &url)
{
    coms_->send(KioskMessage::urlChanged(url));

    // This is the real link clicked
    if (player_)
        player_->play(settings_->linkClickedSound);
}

void Kiosk::elixirMessageReceived(const QString &messageStr)
{
    coms_->send(KioskMessage::channelMessage(messageStr));
}


QRect Kiosk::calculateWindowRect() const
{
    // QList<QScreen*> screens = QApplication::screens();
    // int screenToUse = 0;
    // if (settings_->monitor >= 0 && settings_->monitor < screens.length())
    //     screenToUse = settings_->monitor;

    // QRect screenRect = screens.at(screenToUse)->geometry();

    // if (settings_->fullscreen) {
    //     return screenRect;
    // } else {
    //     int windowWidth = qMax(320, qMin(screenRect.width(), settings_->width));
    //     int windowHeight = qMax(240, qMin(screenRect.height(), settings_->height));
    //     int offsetX = (screenRect.width() - windowWidth) / 2;
    //     int offsetY = (screenRect.height() - windowHeight) / 2;
    //     return QRect(screenRect.x() + offsetX, screenRect.y() + offsetY, windowWidth, windowHeight);
    // }
}