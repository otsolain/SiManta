#pragma once

#include <QScrollArea>
#include <QWidget>
#include <QLayout>
#include <QStyle>
#include <QMap>
#include <QLabel>
#include <QPixmap>

#include "student_tile.h"
#include "protocol.h"

namespace LabMonitor {

class FlowLayout : public QLayout
{
    Q_OBJECT

public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = -1, int hSpacing = -1, int vSpacing = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    int  count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int  heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect& rect) override;

    int horizontalSpacing() const;
    int verticalSpacing() const;

private:
    int doLayout(const QRect& rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem*> m_itemList;
    int m_hSpace;
    int m_vSpace;
};

class StudentGrid : public QWidget
{
    Q_OBJECT

public:
    explicit StudentGrid(QWidget* parent = nullptr);
    StudentTile* addStudent(const StudentInfo& info);
    void removeStudent(const QString& studentId);
    void updateScreenshot(const QString& studentId, const QPixmap& pixmap);
    void setStudentOnline(const QString& studentId, bool online);
    StudentTile* getTile(const QString& studentId) const;
    QList<StudentTile*> allTiles() const;
    void selectAll();
    void deselectAll();
    QList<StudentTile*> selectedTiles() const;
    void setThumbnailSize(int size);
    int studentCount() const { return m_tiles.size(); }
    StudentTile* tileById(const QString& id) const { return m_tiles.value(id, nullptr); }

signals:
    void studentClicked(const QString& studentId);
    void studentDoubleClicked(const QString& studentId);
    void selectionChanged();

private slots:
    void onTileClicked(const QString& studentId);

private:
    void updateTabHeader();
    void updateEmptyState();

    QLabel*       m_tabHeader = nullptr;
    QScrollArea*  m_scrollArea = nullptr;
    QWidget*      m_gridContainer = nullptr;
    FlowLayout*   m_flowLayout = nullptr;
    QLabel*       m_emptyLabel = nullptr;

    QMap<QString, StudentTile*> m_tiles;
    int m_thumbWidth = 240;
};

}
