/*
 * Copyright (c) 2013 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "attachedfiltersmodel.h"
#include "mltcontroller.h"
#include "mainwindow.h"
#include "docks/filtersdock.h"
#include "qmltypes/qmlmetadata.h"
#include <QSettings>
#include <QDebug>

AttachedFiltersModel::AttachedFiltersModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_rows(0)
    , m_dropRow(-1)
{
}

void AttachedFiltersModel::calculateRows()
{
    Mlt::Producer* producer = MLT.producer();
    m_rows =-0;
    if (MLT.isPlaylist()) return;
    if (producer && producer->is_valid()) {
        int n = producer->filter_count();
        while (n--) {
            Mlt::Filter* filter = producer->filter(n);
            if (filter && filter->is_valid() && !filter->get_int("_loader"))
                m_rows++;
            delete filter;
        }
    }
}

Mlt::Filter* AttachedFiltersModel::filterForRow(int row) const
{
    Mlt::Filter* result = 0;
    Mlt::Producer* producer = MLT.producer();
    if (producer && producer->is_valid()) {
        int count = producer->filter_count();
        int j = 0;
        for (int i = 0; i < count; i++) {
            Mlt::Filter* filter = producer->filter(i);
            if (filter && filter->is_valid() && !filter->get_int("_loader")) {
                if (j == row) {
                    result = filter;
                    break;
                }
                j++;
            }
            delete filter;
        }
    }
    return result;
}

int AttachedFiltersModel::indexForRow(int row) const
{
    int result = -1;
    Mlt::Producer* producer = MLT.producer();
    if (producer && producer->is_valid()) {
        int count = producer->filter_count();
        int j = 0;
        for (int i = 0; i < count; i++) {
            Mlt::Filter* filter = producer->filter(i);
            if (filter && filter->is_valid() && !filter->get_int("_loader")) {
                if (j == row) {
                    result = i;
                    break;
                }
                j++;
            }
            delete filter;
        }
    }
    return result;
}

int AttachedFiltersModel::rowCount(const QModelIndex &parent) const
{
    if (MLT.producer() && MLT.producer()->is_valid())
        return m_rows;
    else
        return 0;
}

Qt::ItemFlags AttachedFiltersModel::flags(const QModelIndex &index) const
{
    if (index.isValid())
        return QAbstractListModel::flags(index) | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled;
    else
        return QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;
}

QVariant AttachedFiltersModel::data(const QModelIndex &index, int role) const
{
    if ( !MLT.producer() || !MLT.producer()->is_valid()
        || index.row() >= MLT.producer()->filter_count())
        return QVariant();
    switch (role ) {
    case Qt::DisplayRole: {
            QVariant result;
            Mlt::Filter* filter = filterForRow(index.row());
            if (filter && filter->is_valid()) {
                // Relabel by QML UI
                QmlMetadata* meta = MAIN.filtersDock()->qmlMetadataForService(filter->get("mlt_service"));
                if (meta)
                    result = meta->name();
                // Fallback is raw mlt_service name
                else if (filter->get("mlt_service"))
                    result = QString::fromUtf8(filter->get("mlt_service"));
            }
            delete filter;
            // Relabel for widgets UIs
            if (result == "movit.blur" || result == "boxblur")
                result = tr("Blur");
            else if (result == "movit.lift_gamma_gain" || result == "frei0r.coloradj_RGB")
                result = tr("Color Grading");
            else if (result == "crop")
                result = tr("Crop");
            else if (result == "movit.diffusion")
                result = tr("Diffusion");
            else if (result == "movit.glow" || result == "frei0r.glow")
                result = tr("Glow");
            else if (result == "movit.mirror" || result == "mirror")
                result = tr("Mirror");
            else if (result == "movit.sharpen" || result == "frei0r.sharpness")
                result = tr("Sharpen");
            else if (result == "movit.vignette" || result == "vignette")
                result = tr("Vignette");
            else if (result == "movit.white_balance" || result == "frei0r.colgate")
                result = tr("White Balance");
            return result;
        }
    case Qt::CheckStateRole: {
            Mlt::Filter* filter = filterForRow(index.row());
            QVariant result = Qt::Unchecked;
            if (filter && filter->is_valid() && !filter->get_int("disable"))
                result = Qt::Checked;
            delete filter;
            return result;
        }
        break;
    default:
        break;
    }
    return QVariant();
}

bool AttachedFiltersModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role == Qt::CheckStateRole) {
        Mlt::Filter* filter = filterForRow(index.row());
        if (filter && filter->is_valid()) {
            double speed = MLT.producer()->get_speed();
            MLT.pause();
            filter->set("disable", !filter->get_int("disable"));
            MLT.play(speed);
            emit changed();
            emit dataChanged(createIndex(index.row(), 0), createIndex(index.row(), 0));
        }
        delete filter;
        return true;
    }
    return false;
}

Qt::DropActions AttachedFiltersModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

bool AttachedFiltersModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (MLT.producer() && MLT.producer()->is_valid()) {
        if (m_dropRow == -1)
            m_dropRow = row;
        return true;
    } else {
        return false;
    }
}

bool AttachedFiltersModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (MLT.producer() && MLT.producer()->is_valid() && m_dropRow >= 0 && row != m_dropRow) {
        MLT.pause();
        MLT.producer()->move_filter(indexForRow(row), indexForRow(0) + m_dropRow);
        emit changed();
        emit dataChanged(createIndex(row, 0), createIndex(row, 0));
        emit dataChanged(createIndex(m_dropRow, 0), createIndex(m_dropRow, 0));
        m_dropRow = -1;
        return true;
    } else {
        return false;
    }
}

Mlt::Filter *AttachedFiltersModel::add(const QString& name)
{
    Mlt::Filter* filter = new Mlt::Filter(MLT.profile(), name.toUtf8().constData());
    if (filter->is_valid()) {
        int count = rowCount();
        beginInsertRows(QModelIndex(), count, count);
        MLT.pause();
        MLT.producer()->attach(*filter);
        m_rows++;
        endInsertRows();
        emit changed();
    }
    else qWarning() << "Failed to load filter" << name;
    return filter;
}

void AttachedFiltersModel::remove(int row)
{
    Mlt::Filter* filter = filterForRow(row);
    if (filter && filter->is_valid()) {
        beginRemoveRows(QModelIndex(), row, row);
        MLT.pause();
        MLT.producer()->detach(*filter);
        m_rows--;
        endRemoveRows();
        emit changed();
    }
    delete filter;
}

void AttachedFiltersModel::reset()
{
    calculateRows();
    beginResetModel();
    endResetModel();
}
