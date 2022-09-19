#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>
#include <QDir>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
QT_END_NAMESPACE

//! [0]
class Window : public QWidget
{
  Q_OBJECT

public:
  Window(QWidget *parent = nullptr);

private slots:
  void browse();
  void find();
  void animateFindClick();
  void openFileOfItem(int row, int column);
  void contextMenu(const QPoint &pos);

private:
  QStringList findFiles(const QStringList &files, const QString &text);
  void showFiles(const QStringList &paths);
  QComboBox *createComboBox(const QString &text = QString());
  void createFilesTable();

  QComboBox *fileComboBox;
  QComboBox *textComboBox;
  QComboBox *directoryComboBox;
  QLabel *filesFoundLabel;
  QPushButton *findButton;
  QTableWidget *filesTable;

  QDir currentDir;
};
//! [0]

#endif
