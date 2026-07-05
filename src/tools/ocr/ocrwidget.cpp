// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Contributors

#include "ocrwidget.h"
#include "core/flameshotdaemon.h"
#include "utils/confighandler.h"
#include "utils/globalvalues.h"
#include "widgets/loadspinner.h"
#include "widgets/notificationwidget.h"

#include <QBuffer>
#include <QCursor>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QTextEdit>
#include <QVBoxLayout>

OcrWidget::OcrWidget(const QPixmap& capture, QWidget* parent)
  : QWidget(parent)
  , m_pixmap(capture)
{
    setWindowTitle(tr("OCR - Text Extraction"));
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setAttribute(Qt::WA_DeleteOnClose);

    QRect position = frameGeometry();
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    position.moveCenter(screen->availableGeometry().center());
    move(position.topLeft());

    m_layout = new QVBoxLayout(this);

    m_spinner = new LoadSpinner(this);
    m_spinner->setColor(ConfigHandler().uiColor());
    m_spinner->start();

    m_statusLabel = new QLabel(tr("Extracting text..."));
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setVisible(false);

    m_copyButton = new QPushButton(tr("Copy to Clipboard"), this);
    m_copyButton->setVisible(false);
    connect(m_copyButton, &QPushButton::clicked, this, &OcrWidget::copyToClipboard);

    m_notification = nullptr;

    m_layout->addWidget(m_spinner, 0, Qt::AlignHCenter);
    m_layout->addWidget(m_statusLabel);
    m_layout->addWidget(m_textEdit);
    m_layout->addWidget(m_copyButton, 0, Qt::AlignHCenter);

    resize(500, 400);

    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &OcrWidget::handleReply);

    startOcr();
    new QShortcut(Qt::Key_Escape, this, SLOT(close()));
}

void OcrWidget::startOcr()
{
    // Encode pixmap to base64 PNG
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    m_pixmap.save(&buffer, "PNG");
    QString base64Image = QString::fromLatin1(byteArray.toBase64());

    // Build JSON request body for Ollama /api/chat
    QJsonObject contentObj;
    contentObj[QStringLiteral("role")] = QStringLiteral("user");
    contentObj[QStringLiteral("content")] = QStringLiteral(
        "Extract all text from this image. Return only the extracted text, nothing else.");

    QJsonArray imagesArray;
    imagesArray.append(base64Image);
    contentObj[QStringLiteral("images")] = imagesArray;

    QJsonArray messagesArray;
    messagesArray.append(contentObj);

    QJsonObject requestBody;
    requestBody[QStringLiteral("model")] = ConfigHandler().ocrModel();
    requestBody[QStringLiteral("messages")] = messagesArray;
    requestBody[QStringLiteral("stream")] = false;

    QJsonDocument doc(requestBody);

    // Send request
    QUrl url(ConfigHandler().ocrEndpoint());
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setTransferTimeout(120000);

    m_networkManager->post(request, doc.toJson());
}

void OcrWidget::handleReply(QNetworkReply* reply)
{
    m_spinner->stop();
    m_spinner->deleteLater();
    m_spinner = nullptr;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
        QJsonObject json = response.object();
        QJsonObject message = json[QStringLiteral("message")].toObject();
        QString content = message[QStringLiteral("content")].toString();

        if (content.isEmpty()) {
            m_statusLabel->setText(tr("No text found in the image."));
        } else {
            m_statusLabel->setText(tr("Text extracted successfully:"));
            m_textEdit->setPlainText(content.trimmed());
            m_textEdit->setVisible(true);
            m_copyButton->setVisible(true);
        }
    } else {
        m_statusLabel->setText(
            tr("OCR request failed: %1\n\nMake sure Ollama is running at %2")
                .arg(reply->errorString(),
                     ConfigHandler().ocrEndpoint()));
    }
    reply->deleteLater();
}

void OcrWidget::copyToClipboard()
{
    QString text = m_textEdit->toPlainText();
    FlameshotDaemon::copyToClipboard(
        text, tr("OCR text copied to clipboard."));

    if (!m_notification) {
        m_notification = new NotificationWidget(this);
        m_layout->addWidget(m_notification);
    }
    m_notification->showMessage(tr("OCR text copied to clipboard."));
}
