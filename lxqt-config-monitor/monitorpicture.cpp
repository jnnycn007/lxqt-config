/*
 *    Copyright (C) 2014  P.L. Lucas <selairi@gmail.com>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "monitorpicture.h"

#include <QFont>
#include <QFontMetrics>
#include <QPen>
#include <QDebug>
#include <QVector2D>
#include <QRectF>
#include <KScreen/Mode>
#include <QScrollBar>
#include <QResizeEvent>
#include <QTransform>

#include <cmath>
#include <algorithm>

#include "configure.h"

// Gets size from string rate. String rate format is "widthxheight". Example: 800x600
static QSize sizeFromString(QString str)
{
    int width = 0;
    int height = 0;
    int x = str.indexOf(QLatin1Char('x'));
    if (x > 0) {
        width = str.first(x).toInt();
        height = str.last(str.size() - x - 1).toInt();
    }
    return QSize(width, height);
}

MonitorPictureProxy::MonitorPictureProxy(QObject *parent, MonitorPicture *monitorPicture):QObject(parent)
{
    this->monitorPicture = monitorPicture;
}

void MonitorPictureProxy::updateSize()
{
    KScreen::OutputPtr output = monitorPicture->monitorWidget->output;
    QSize size = output->currentMode()->size();
    monitorPicture->updateSize(size);
}

void MonitorPictureProxy::updatePosition()
{
    KScreen::OutputPtr output = monitorPicture->monitorWidget->output;
    QPoint pos = output->pos();
    //qDebug() << "MonitorPictureProxy:updatePosition]" << pos;
    monitorPicture->setMonitorPosition(pos.x(), pos.y());
}

MonitorPictureDialog::MonitorPictureDialog(KScreen::ConfigPtr config, QWidget * parent, Qt::WindowFlags f) :
    QWidget(parent, f)
{
    updatingOk = false;
    firstShownOk = false;
    maxMonitorSize = 0;
    mConfig = config;
    ui.setupUi(this);
}


void MonitorPictureDialog::setScene(QList<MonitorWidget *> monitors)
{
    int monitorsWidth =0;
    int monitorsHeight = 0;
    QGraphicsScene *scene = new QGraphicsScene(this);
    for (MonitorWidget *monitor : monitors) {
        MonitorPicture *monitorPicture = new MonitorPicture(nullptr, monitor, this);
        pictures.append(monitorPicture);
        scene->addItem(monitorPicture);
        monitorsWidth += monitorPicture->rect().width();
        monitorsHeight += monitorPicture->rect().height();
        MonitorPictureProxy *proxy = new MonitorPictureProxy(this, monitorPicture);
        proxy->connect(monitor->output.data(), &KScreen::Output::currentModeIdChanged, proxy, &MonitorPictureProxy::updateSize);
        proxy->connect(monitor->output.data(), &KScreen::Output::posChanged, proxy, &MonitorPictureProxy::updatePosition);
    }
    // The blue rectangle is maximum size of virtual screen (framebuffer)
    scene->addRect(0, 0, mConfig->screen()->maxSize().width(), mConfig->screen()->maxSize().height(), QPen(Qt::blue, 20))->setOpacity(0.5);
    maxMonitorSize = std::max(monitorsWidth, monitorsHeight);
    ui.graphicsView->setScene(scene);
}

void MonitorPictureDialog::showEvent(QShowEvent * event)
{
    QWidget::showEvent(event);
    if (!firstShownOk) {
        firstShownOk = true;
        qreal minWidgetLength = static_cast<qreal>(std::max(ui.graphicsView->size().width(), ui.graphicsView->size().height())) / 1.2;
        if (maxMonitorSize > 0)
            updateScale(minWidgetLength / maxMonitorSize);
    }
}

void MonitorPictureDialog::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (firstShownOk && maxMonitorSize > 0)
    {
        qreal scale = ui.graphicsView->transform().m11();
        if (scale > 0)
        {
            qreal minWidgetLength = static_cast<qreal>(std::max(ui.graphicsView->size().width(), ui.graphicsView->size().height())) / 1.2;
            updateScale((minWidgetLength / maxMonitorSize) / scale);
        }
    }
}

void MonitorPictureDialog::updateScale(qreal scale)
{
    // Update scale and set scrollbar position.
    if (scale > 0)
    {
        ui.graphicsView->scale(scale, scale);
        updateScene();
        ui.graphicsView->verticalScrollBar()->setValue(0);
        ui.graphicsView->horizontalScrollBar()->setValue(0);
    }
}

void MonitorPictureDialog::updateScene()
{
    ui.graphicsView->scene()->update();
}

void MonitorPictureDialog::updateMonitorWidgets(QString primaryMonitor)
{
    // This method update spin boxes of position.
    // If position is changed when this method is running, position is changed until buffer overflow.
    // updatingOk control that this method can not be run twice in the same position change.

    if(updatingOk)
        return;
    updatingOk = true;
    int x0, y0;
    x0 = y0 = 0;

    for (MonitorPicture *picture : std::as_const(pictures)) {
        if (picture->monitorWidget->output->name() == primaryMonitor
                || primaryMonitor == QLatin1String()) {
            x0 = picture->originX + picture->pos().x();
            y0 = picture->originY + picture->pos().y();
            break;
        }
    }

    if( primaryMonitor == QLatin1String() ) {
        for(MonitorPicture *picture : std::as_const(pictures)) {
            int x1 = picture->originX + picture->pos().x();
            int y1 = picture->originY + picture->pos().y();
            x0 = std::min(x0, x1);
            y0 = std::min(y0, y1);
        }
    }

    for (MonitorPicture *picture : std::as_const(pictures)) {
        int x = picture->originX + picture->pos().x() - x0;
        int y = picture->originY + picture->pos().y() - y0;
        if( x != picture->monitorWidget->ui.xPosSpinBox->value() )
            picture->monitorWidget->ui.xPosSpinBox->setValue(x);
        //else
        //    qDebug() << "x Iguales";
        if( y != picture->monitorWidget->ui.yPosSpinBox->value() )
            picture->monitorWidget->ui.yPosSpinBox->setValue(y);
        //else
        //    qDebug() << "y Iguales";
        //qDebug() << "[MonitorPictureDialog::updateMonitorWidgets]" << x << '=' << picture->monitorWidget->ui.xPosSpinBox->value() << ',' << y << '=' << picture->monitorWidget->ui.yPosSpinBox->value();
    }
    updatingOk = false;
}

MonitorPicture::MonitorPicture(QGraphicsItem * parent,
                               MonitorWidget *monitorWidget,
                               MonitorPictureDialog *monitorPictureDialog) :
    QGraphicsRectItem(parent)
{
    this->monitorWidget = monitorWidget;
    this->monitorPictureDialog = monitorPictureDialog;
    QSizeF currentSizeF = sizeFromString(monitorWidget->ui.resolutionCombo->currentText()) / monitorWidget->output->scale();
    QSize currentSize = currentSizeF.toSize();
    if( monitorWidget->output->rotation() == KScreen::Output::Left || monitorWidget->output->rotation() == KScreen::Output::Right )
        currentSize.transpose();
    int x = monitorWidget->ui.xPosSpinBox->value();
    int y = monitorWidget->ui.yPosSpinBox->value();
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges);
    originX = x;
    originY = y;

    setRect(x, y, currentSize.width(), currentSize.height());
    // setPen(QPen(Qt::black, 20));
    // textItem = new QGraphicsTextItem(monitorWidget->output->name(), this);
    // textItem->setX(x);
    // textItem->setY(y);
    // textItem->setParentItem(this);

    QSvgRenderer *renderer = new QSvgRenderer(QLatin1String(ICON_PATH "monitor.svg"));
    svgItem = new QGraphicsSvgItem();
    svgItem->setSharedRenderer(renderer);
    svgItem->setX(x);
    svgItem->setY(y);
    svgItem->setOpacity(0.7);
    svgItem->setParentItem(this);


    textItem = new QGraphicsTextItem(monitorWidget->output->name(), this);
    textItem->setDefaultTextColor(Qt::white);
    textItem->setX(x);
    textItem->setY(y);
    textItem->setParentItem(this);
    setPen(QPen(Qt::black, 20));


    adjustNameSize();
}

void MonitorPicture::adjustNameSize()
{
    prepareGeometryChange();
    qreal fontWidth = QFontMetrics(textItem->font()).horizontalAdvance(monitorWidget->output->name() + QStringLiteral("  "));
    textItem->setScale((qreal) this->rect().width() / fontWidth);
    QTransform transform;
    qreal width = std::abs(this->rect().width()/svgItem->boundingRect().width());
    qreal height = std::abs(this->rect().height()/svgItem->boundingRect().height());
    qDebug() << "Width x Height" << width << "x" << height;
    transform.scale(width, height);
    svgItem->setTransform(transform);
}

void MonitorPicture::updateSize(QSize currentSize)
{
    QRectF r = rect();
    r.setSize(currentSize);
    setRect(r);
    adjustNameSize();
}

QVariant MonitorPicture::itemChange(GraphicsItemChange change, const QVariant & value)
{
    //qDebug() << "[MonitorPicture::itemChange]: ";
    //if ( change == ItemPositionChange && scene()) {
    // value is the new position.
    //QPointF newPos = value.toPointF();
    //qDebug() << "[MonitorPictureDialog::updateMonitorWidgets]: " << newPos.x() << "x" << newPos.y();
    //}
    QVariant v = QGraphicsItem::itemChange(change, value);
    //monitorPictureDialog->updateMonitorWidgets(QString());
    return v;
}

void MonitorPicture::setMonitorPosition(int x, int y)
{
    setX( x - originX );
    setY( y - originY );
}

void MonitorPicture::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
    QGraphicsRectItem::mouseReleaseEvent(event);
    monitorPictureDialog->moveMonitorPictureToNearest(this);
    monitorPictureDialog->updateMonitorWidgets(QString());
}

//////////////////////////////////////////////////////////////////////////////////
// Move picture to nearest picture procedure.
//////////////////////////////////////////////////////////////////////////////////

struct Result_moveMonitorPictureToNearest {
    bool ok = false;
    bool outside = false;
    QVector2D vector;
};

static Result_moveMonitorPictureToNearest compareTwoMonitors(MonitorPicture* monitorPicture1,
        MonitorPicture* monitorPicture2)
{
    Result_moveMonitorPictureToNearest result;
    QVector2D offsetVector(0, 0);
    QRectF extendedAreaRect;
    QRectF monitorPicture1Rect(
        monitorPicture1->x() + monitorPicture1->originX,
        monitorPicture1->y() + monitorPicture1->originY,
        monitorPicture1->rect().width(),
        monitorPicture1->rect().height());
    QRectF monitorPicture2Rect(
        monitorPicture2->x() + monitorPicture2->originX,
        monitorPicture2->y() + monitorPicture2->originY,
        monitorPicture2->rect().width(),
        monitorPicture2->rect().height());

    if(monitorPicture1Rect.intersects(monitorPicture2Rect)) {
        result.ok = true;
        return result;
    }

    result.outside = true;
    result.ok = false;

    extendedAreaRect = QRectF(
                           std::min(monitorPicture2Rect.x(), monitorPicture1Rect.x()) - monitorPicture2Rect.width(),
                           monitorPicture2Rect.y(),
                           std::max(monitorPicture2Rect.x(), monitorPicture1Rect.x()) + 2*monitorPicture2Rect.width(),
                           monitorPicture2Rect.height());

    //qDebug() << "\nextendedAreaRect: " << extendedAreaRect;
    //qDebug() << "monitorPicture1Rect: " << monitorPicture1Rect << monitorPicture1->rect().width() << monitorPicture1->rect().height();
    //qDebug() << "monitorPicture2Rect: " << monitorPicture2Rect;

    if(extendedAreaRect.intersects(monitorPicture1Rect)) {
        // monitorPicture1 left
        offsetVector = QVector2D(monitorPicture2Rect.right() - monitorPicture1Rect.left(), 0);
        result.vector = offsetVector;

        // monitorPicture1 right
        offsetVector = QVector2D(monitorPicture2Rect.left() - monitorPicture1Rect.right(), 0);
        if(result.vector.length() > offsetVector.length())
            result.vector = offsetVector;

        float y2 = monitorPicture2Rect.top();
        float y1 = monitorPicture1Rect.top();
        float delta = monitorPicture2Rect.height() * 0.1;
        if(y2 < y1 && y1 < (y2+delta))
            result.vector.setY(y2 - y1);
        else {
            y2 = monitorPicture2Rect.bottom();
            y1 = monitorPicture1Rect.bottom();
            if((y2 - delta) < y1 && y1 < y2)
                result.vector.setY(y2 - y1);
        }

        result.outside = false;
    }

    extendedAreaRect = QRectF(
                           monitorPicture2Rect.x(),
                           std::min(monitorPicture2Rect.y(), monitorPicture1Rect.y()) - monitorPicture2Rect.height(),
                           monitorPicture2Rect.width(),
                           std::max(monitorPicture2Rect.y(), monitorPicture1Rect.y()) + 2*monitorPicture2Rect.height()
                       );

    if(extendedAreaRect.intersects(monitorPicture1Rect)) {
        // monitorPicture1 top
        offsetVector = QVector2D(0, monitorPicture2Rect.bottom() - monitorPicture1Rect.top());
        result.vector = offsetVector;

        // monitorPicture1 bottom
        offsetVector = QVector2D(0, monitorPicture2Rect.top() - monitorPicture1Rect.bottom());
        if(result.vector.length() > offsetVector.length())
            result.vector = offsetVector;

        float x2 = monitorPicture2Rect.left();
        float x1 = monitorPicture1Rect.left();
        float delta = monitorPicture2Rect.width() * 0.1;
        if(x2 < x1 && x1 < (x2+delta))
            result.vector.setX(x2 - x1);
        else {
            x2 = monitorPicture2Rect.right();
            x1 = monitorPicture1Rect.right();
            if((x2 - delta) < x1 && x1 < x2)
                result.vector.setX(x2 - x1);
        }

        result.outside = false;
    }

    return result;
}


void MonitorPictureDialog::moveMonitorPictureToNearest(MonitorPicture* monitorPicture)
{
    if (!ui.magneticCheckBox->isChecked())
        return;

    // Float to int
    monitorPicture->setX(static_cast<qreal>(std::round(monitorPicture->x())));
    monitorPicture->setY(static_cast<qreal>(std::round(monitorPicture->y())));


    QVector2D vector(0, 0);
    for (MonitorPicture *picture : std::as_const(pictures)) {
        if (picture == monitorPicture)
            continue;

        // Float to int. The positions of the Monitors must be set with pixels.
        // QGraphicsView uses float to store x and y. Then, positions as (800.5, 600.3) are stored.
        // x and y have to be translated from float to int in order to store pixels position:
        picture->setX(static_cast<qreal>(std::round(picture->x())));
        picture->setY(static_cast<qreal>(std::round(picture->y())));

        Result_moveMonitorPictureToNearest result = compareTwoMonitors(monitorPicture, picture);
        if (result.ok)
            return;
        else if (! result.outside && (result.vector.length() < vector.length() || vector.length() == 0.0))
            vector = result.vector;
    }

    int x = monitorPicture->x();
    int y = monitorPicture->y();
    monitorPicture->setX(x + vector.x());
    monitorPicture->setY(y + vector.y());
}
