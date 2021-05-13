#include "musictable.h"
#include "lib/powerpcasm.h"
#include "lib/vanilladatabase.h"


quint32 MusicTable::writeTable(const QVector<MapDescriptor> &descriptors) {
    QVector<quint32> table;
    for (auto &descriptor: descriptors) {
        QVector<quint32> mapMusicTable;
        auto keys = descriptor.music.keys();
        mapMusicTable.append(keys.size());
        for (auto &musicType: keys) {
            mapMusicTable.append(musicType);
            mapMusicTable.append(descriptor.music[musicType].brsarIndex);
        }
        if(descriptor.music.empty()) {
            table.append(0);
        } else {
            table.append(allocate(mapMusicTable, "MapMusicReplacementTable"));
        }
    }
    return allocate(table, "MapMusicPointerTable");
}


void MusicTable::writeAsm(QDataStream &stream, const AddressMapper &addressMapper, const QVector<MapDescriptor> &mapDescriptors) {
    quint32 tableAddr = writeTable(mapDescriptors);

    // Hijack Game::ConvBGMID(int bgmId = r3, int theme = r4)
    quint32 hijackAddr = addressMapper.boomStreetToStandard(0x801cc8a0);
    quint32 returnContinueAddr = addressMapper.boomStreetToStandard(0x801cc8a4);
    quint32 returnBgmReplacedAddr = addressMapper.boomStreetToStandard(0x801cc93c);

    quint32 subroutineReplaceBgmId = allocate(writeSubroutineReplaceBgmId(addressMapper, tableAddr, 0, returnContinueAddr, returnBgmReplacedAddr), "SubroutineReplaceBgmId");
    stream.device()->seek(addressMapper.toFileAddress(subroutineReplaceBgmId));
    auto insts = writeSubroutineReplaceBgmId(addressMapper, tableAddr, subroutineReplaceBgmId, returnContinueAddr, returnBgmReplacedAddr); // re-write the routine again since now we know where it is located in the main dol
    for (quint32 inst: qAsConst(insts)) stream << inst;
    stream.device()->seek(addressMapper.toFileAddress(hijackAddr));
    // mr r31, r3                                   -> b subroutineReplaceBgmId
    stream.device()->seek(addressMapper.boomToFileAddress(0x801cc8a0));
    stream << PowerPcAsm::b(addressMapper.boomStreetToStandard(0x801cc8a0), subroutineReplaceBgmId);
}

QVector<quint32> MusicTable::writeSubroutineReplaceBgmId(const AddressMapper &addressMapper, quint32 tableAddr, quint32 entryAddr, quint32 returnContinueAddr, quint32 returnBgmReplacedAddr) {
    quint32 Game_Manager = addressMapper.boomStreetToStandard(0x8081794c);
    quint32 Global_MapID = addressMapper.boomStreetToStandard(0x80552408);
    PowerPcAsm::Pair16Bit g = PowerPcAsm::make16bitValuePair(Game_Manager);
    PowerPcAsm::Pair16Bit m = PowerPcAsm::make16bitValuePair(Global_MapID);
    PowerPcAsm::Pair16Bit t = PowerPcAsm::make16bitValuePair(tableAddr);

    QVector<quint32> asm_;
    // precondition:   r3 is bgmId
    //                 r4 is theme
    //        unused:  r0,r5,r6,r7,r31
    // postcondition:  r3 and r31 is bgmId
    asm_.append(PowerPcAsm::mr(31, 3));             // r31 <- r3
    asm_.append(PowerPcAsm::lis(3, g.upper));       // \.
    asm_.append(PowerPcAsm::addi(3, 3, g.lower));   // |.
    asm_.append(PowerPcAsm::lwz(5, 0x0, 3));        // /. r5 <- Game_Manager
    asm_.append(PowerPcAsm::cmpwi(5, 0x0));         // r5 != 0?
    asm_.append(PowerPcAsm::bne(4));                // continue
    asm_.append(PowerPcAsm::mr(3, 31));             // r3 <- r31
    asm_.append(PowerPcAsm::cmplwi(3, 0xffff));     // make the comparision again from the original function
    asm_.append(PowerPcAsm::b(entryAddr, asm_.size(), returnContinueAddr));    // else return returnContinueAddr
    asm_.append(PowerPcAsm::lis(3, m.upper));       // \.
    asm_.append(PowerPcAsm::addi(3, 3, m.lower));   // |.
    asm_.append(PowerPcAsm::lwz(5, 0x0, 3));        // /. r5 <- Global_MapID
    asm_.append(PowerPcAsm::li(3, 3));              // \.
    asm_.append(PowerPcAsm::slw(5, 5, 3));          // /. r5 <- r5 * 8
    asm_.append(PowerPcAsm::lis(3, t.upper));       // \.
    asm_.append(PowerPcAsm::addi(3, 3, t.lower));   // |.
    asm_.append(PowerPcAsm::lwzx(5, 5, 3));         // /. r5 <- MapMusicReplacementTable
    asm_.append(PowerPcAsm::cmpwi(5, 0x0));         // r5 != 0?
    asm_.append(PowerPcAsm::bne(4));                // continue
    asm_.append(PowerPcAsm::mr(3, 31));             // r3 <- r31
    asm_.append(PowerPcAsm::cmplwi(3, 0xffff));     // make the comparision again from the original function
    asm_.append(PowerPcAsm::b(entryAddr, asm_.size(), returnContinueAddr));    // else return returnContinueAddr
    asm_.append(PowerPcAsm::lwz(6, 0x0, 5));        // r6 <- size of MapMusicReplacementTable
    asm_.append(PowerPcAsm::addi(5, 5, 0x4));       // r5+=4
    int whileVentureCardIdSmaller128 = asm_.size();
    {
        asm_.append(PowerPcAsm::lwz(3, 0x0, 5));    // r3 <- firstBgmId
        asm_.append(PowerPcAsm::cmpw(3, 31));       // r3 == r31?
        asm_.append(PowerPcAsm::bne(6));            // {
        asm_.append(PowerPcAsm::addi(5, 5, 0x4));   // r5+=4
        asm_.append(PowerPcAsm::lwz(31, 0x0, 5));   // r31 <- secondBgmId
        asm_.append(PowerPcAsm::mr(3, 31));         // r3 <- r31
        asm_.append(PowerPcAsm::cmplwi(3, 0xffff)); // make the comparision again from the original function
        asm_.append(PowerPcAsm::b(entryAddr, asm_.size(), returnBgmReplacedAddr));     // return returnBgmReplacedAddr
                                                    // }
        asm_.append(PowerPcAsm::addi(5, 5, 0x8));   // r5+=8
        asm_.append(PowerPcAsm::subi(6, 6, 0x1));   // r6--
        asm_.append(PowerPcAsm::cmpwi(6, 0x0));     // r6 != 0?
        asm_.append(PowerPcAsm::bne(asm_.size(), whileVentureCardIdSmaller128));   // loop
        asm_.append(PowerPcAsm::mr(3, 31));         // r3 <- r31
        asm_.append(PowerPcAsm::cmplwi(3, 0xffff)); // make the comparision again from the original function
        asm_.append(PowerPcAsm::b(entryAddr, asm_.size(), returnContinueAddr));    // else return returnContinueAddr
    }
    return asm_;
}

void MusicTable::readAsm(QDataStream &, QVector<MapDescriptor> &, const AddressMapper &, bool ) {
    // TODO
}

quint32 MusicTable::readTableAddr(QDataStream &, const AddressMapper &, bool) {
    // TODO
    return 0;
}

qint16 MusicTable::readTableRowCount(QDataStream &, const AddressMapper &, bool) {
    return -1;
}

bool MusicTable::readIsVanilla(QDataStream &stream, const AddressMapper &addressMapper) {
    stream.device()->seek(addressMapper.boomToFileAddress(0x801cc8a0));
    quint32 opcode; stream >> opcode;
    return opcode == PowerPcAsm::mr(31, 3);
}

