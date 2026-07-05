// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Contributors

#pragma once

#include <QPixmap>
#include <QWidget>

class QTextEdit;
class QPushButton;
class QVBoxLayout;
class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class LoadSpinner;
class NotificationWidget;

class OcrWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OcrWidget(const QPixmap& capture, QWidget* parent = nullptr);

public slots:
    void copyToClipboard();

private slots:
    void handleReply(QNetworkReply* reply);

private:
    void startOcr();

    QPixmap m_pixmap;
    QVBoxLayout* m_layout;
    QLabel* m_statusLabel;
    LoadSpinner* m_spinner;
    QTextEdit* m_textEdit;
    QPushButton* m_copyButton;
    QNetworkAccessManager* m_networkManager;
    NotificationWidget* m_notification;
};
