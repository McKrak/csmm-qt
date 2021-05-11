#include "brsar.h"
#include <QtDebug>

// documentation:
//   http://wiki.tockdom.com/wiki/BRSAR_(File_Format)
//   https://github.com/soopercool101/BrawlCrate/blob/master/BrawlLib/SSBB/Types/Audio/RSAR.cs

namespace Brsar {

QDataStream &operator>>(QDataStream &stream, Brsar::SectionHeader &data) {
    char header[4];
    stream.readRawData(header, 4);
    if (QByteArray(header, 4) != data.magicNumber) {
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> data.sectionSize;
    data.sectionStart = stream.device()->pos();
    return stream;
}

QDataStream &operator>>(QDataStream &stream, Brsar::InfoSectionCollectionTable &data) {
    return stream;
}

QDataStream &operator>>(QDataStream &stream, Brsar::InfoSectionSoundDataEntry &data) {
    stream >> data.fileNameIndex;
    stream >> data.fileCollectionIndex;
    stream >> data.playerId;
    quint32 unknown;
    stream >> unknown;
    if(unknown != data.unknown1){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> data.offsetThirdSubsection; // we do not actually need it, since it always comes directly after secondSubsection
    stream >> data.volume;
    stream >> data.playerPriority;
    stream >> data.soundType;
    stream >> data.remoteFilter;
    stream >> data.unknownFlags;
    stream >> data.offsetSecondSubsection; // we do not actually need it, since it always comes directly after this dataEntry
    stream >> data.userParam1;
    stream >> data.userParam2;
    stream >> data.panMode;
    stream >> data.panCurve;
    stream >> data.actorPlayerId;
    quint8 unknown2;
    stream >> unknown2;
    if(unknown2 != data.unknown2){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    // -- sound data subsection 2 --
    // note: we always assume RSTM, as such this brsar parser is incomplete and only for
    //       CSMM relevant
    stream >> data.startPosition;
    stream >> data.allocChannelCount;
    stream >> data.rstmAllocTrack;
    stream >> data.unknown3;
    // -- sound data subsection 3 --
    stream >> data.sound3dParamFlags;
    stream >> data.decayCurve;
    stream >> data.decayRatio;
    return stream;
}

QDataStream &operator>>(QDataStream &stream, Brsar::InfoSectionSoundDataTable &data) {
    stream >> data.soundTableSize;

    for(quint32 i = 0; i < data.soundTableSize; i++) {
        quint32 unknown;
        stream >> unknown;
        if(unknown != data.unknown){
            stream.setStatus(QDataStream::ReadCorruptData);
            return stream;
        }
        quint32 soundTableEntryOffset;
        stream >> soundTableEntryOffset;
        data.soundTableEntryOffsets.append(soundTableEntryOffset);
    }
    for(quint32 i = 0; i < data.soundTableSize; i++) {
        stream.device()->seek(data.header->sectionStart + data.soundTableEntryOffsets[i]);
        qDebug() << data.header->sectionStart + data.soundTableEntryOffsets[i];
        InfoSectionSoundDataEntry soundDataEntry;
        stream >> soundDataEntry;
        data.soundTableEntries.append(soundDataEntry);
    }
    return stream;
}

QDataStream &operator>>(QDataStream &stream, Brsar::InfoSection &data) {
    stream >> data.header;

    quint32 unknown;
    stream >> unknown;
    if(unknown != data.unknown1){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> data.offsetSoundDataTable;
    stream >> unknown;
    if(unknown != data.unknown2){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> data.offsetSoundbankTable;
    stream >> unknown;
    if(unknown != data.unknown3){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> data.offsetPlayerInfoTable;
    stream >> unknown;
    if(unknown != data.unknown4){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> data.offsetCollectionTable;
    stream >> unknown;
    if(unknown != data.unknown5){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> data.offsetGroupTable;
    stream >> unknown;
    if(unknown != data.unknown6){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream.device()->seek(data.header.sectionStart + data.offsetSoundDataTable);
    stream >> data.soundDataTable;
    stream.device()->seek(data.header.sectionStart + data.offsetCollectionTable);
    stream >> data.collectionTable;
    return stream;
}

QDataStream &operator>>(QDataStream &stream, Brsar::SymbSectionFileNameTable &data) {
    stream >> data.fileNameTableSize;

    for(quint32 i = 0; i < data.fileNameTableSize; i++) {
        quint32 fileNameTableEntryOffset;
        stream >> fileNameTableEntryOffset;
        data.fileNameTableEntryOffsets.append(fileNameTableEntryOffset);
    }
    for(quint32 i = 0; i < data.fileNameTableSize; i++) {
        stream.device()->seek(data.header->sectionStart + data.fileNameTableEntryOffsets[i]);
        char buf[128+1] = {0};
        stream.readRawData(buf, 128);
        data.fileNameTableEntries.append(buf);
    }
    // How to write a string:
    /*  QByteArray data(str.toUtf8());
        data.append('\0');
        stream.writeRawData(bytes, bytes.size());  */

    return stream;
}

QDataStream &operator>>(QDataStream &stream, Brsar::SymbSection &data) {
    stream >> data.header;
    stream >> data.offsetFileNameTable;
    stream >> data.maskTableOffset1;
    stream >> data.maskTableOffset2;
    stream >> data.maskTableOffset3;
    stream >> data.maskTableOffset4;
    stream.device()->seek(data.header.sectionStart + data.offsetFileNameTable);
    stream >> data.fileNameTable;
    return stream;
}

QDataStream &operator>>(QDataStream &stream, Brsar::File &data) {
    char header[4];
    stream.readRawData(header, 4);
    if (QByteArray(header, 4) != data.magicNumber) {
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    quint16 byteOrderMark;
    stream >> byteOrderMark;
    if(byteOrderMark != data.byteOrderMark){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    quint16 fileFormatVersion;
    stream >> fileFormatVersion;
    if(fileFormatVersion != data.fileFormatVersion){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> data.fileLength;
    quint16 headerSize;
    stream >> headerSize;
    if(headerSize != data.headerSize){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    quint16 sectionCount;
    stream >> sectionCount;
    if(sectionCount != data.sectionCount){
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> data.symbOffset;
    stream >> data.symbLength;
    stream >> data.infoOffset;
    stream >> data.infoLength;
    stream >> data.fileOffset;
    stream >> data.fileLength;
    stream.skipRawData(24);
    stream.device()->seek(data.symbOffset);
    stream >> data.symb;
    stream.device()->seek(data.infoOffset);
    stream >> data.info;
    return stream;
}

void patch(QDataStream &stream, const QVector<MapDescriptor> &mapDescriptors) {
    Brsar::File brsar;
    stream >> brsar;

}

}
