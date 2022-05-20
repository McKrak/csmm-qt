#include "pythonbindings.h"
#include <pybind11/embed.h>
#include <pybind11/stl_bind.h>
#include "lib/mods/csmmmod.h"

static std::ostream &operator<<(std::ostream &stream, const QString &str) {
    return stream << str.toStdString();
}

template<class ...Args>
static void defineContainerRepr(Args&&... args) {
}

template<class Cont>
static auto defineContainerRepr(pybind11::class_<Cont> &cls, const std::string &name)
    -> decltype(std::declval<std::ostringstream>() << std::declval<Cont>().front(), void()) {
    cls.def("__repr__", [name](const Cont &v) {
        std::ostringstream ss;
        ss << name << "[";
        for (auto it = v.begin(); it != v.end(); ++it) {
            ss << *it;
            if (std::next(it) != v.end()) {
                ss << ", ";
            }
        }
        ss << "]";
        return ss.str();
    }, "Return the canonical string representation of this list.");
}

template<class Cont>
static void bindConstContainerOps(pybind11::class_<Cont> &cls, const std::string &name) {
    using ConstTRef = decltype(std::declval<const Cont>().front());
    cls.def(
            "__bool__",
            [](const Cont &v) -> bool { return !v.empty(); },
            "Check whether the list is nonempty");
    cls.def("__len__", &Cont::size);
    cls.def("__contains__", [](const Cont &v, ConstTRef x) { return std::find(v.begin(), v.end(), x) != v.end(); });
    defineContainerRepr(cls, name);
}

template<class T, size_t N, class ...Args>
static pybind11::class_<std::array<T, N>> bindStdArray(pybind11::handle h, const std::string &name, Args &&...args) {
    pybind11::class_<std::array<T, N>> cls(h, name.c_str(), std::forward<Args>(args)...);
    cls.def(pybind11::init<>());
    cls.def(pybind11::init([](const pybind11::iterable &it) {
        std::array<T, N> res;
        size_t i = 0;
        for (auto &val: it) {
            if (i >= N) throw pybind11::value_error("iterable too large");
            res[i++] = val.cast<T>();
        }
        if (i < N) throw pybind11::value_error("iterable too small");
        return res;
    }));
    pybind11::detail::vector_if_copy_constructible<std::array<T, N>, decltype(cls)>(cls);
    pybind11::detail::vector_accessor<std::array<T, N>, decltype(cls)>(cls);
    bindConstContainerOps(cls, name);
    return cls;
}

namespace {

class PyQIODevice : public QIODevice {
public:
    PyQIODevice(pybind11::object obj) : obj(obj) {}
    bool isSequential() const override {
        return obj.attr("seekable")().cast<bool>();
    }
    bool seek(qint64 pos) override {
        obj.attr("seek")(pos);
        return true;
    }
protected:
    qint64 readData(char *data, qint64 maxlen) override {
        pybind11::bytes readData = obj.attr("read")(maxlen);
        char *dataToCopyFrom;
        pybind11::ssize_t size;
        if (PyBytes_AsStringAndSize(readData.ptr(), &dataToCopyFrom, &size) < 0) {
            throw pybind11::error_already_set();
        }
        memcpy(data, dataToCopyFrom, size);
        return size;
    }
    qint64 writeData(const char *data, qint64 len) override {
        return obj.attr("write")(pybind11::bytes(data, len)).cast<qint64>();
    }
private:
    pybind11::object obj;
};

class PyCSMMMod : public CSMMMod {
public:
    using CSMMMod::CSMMMod;

    QString modId() const override {
        PYBIND11_OVERRIDE_PURE(QString, CSMMMod, modId);
    }
    int priority() const override {
        PYBIND11_OVERRIDE(int, CSMMMod, priority);
    }
    QSet<QString> depends() const override {
        PYBIND11_OVERRIDE(QSet<QString>, CSMMMod, depends);
    }
    QSet<QString> after() const override {
        PYBIND11_OVERRIDE(QSet<QString>, CSMMMod, after);
    }
};

class PyArcFileInterface : public ArcFileInterface {
private:
    typedef QMap<QString, ModifyArcFunction> ResultType;
public:
    using ArcFileInterface::ArcFileInterface;

    QMap<QString, ModifyArcFunction> modifyArcFile() override {
        PYBIND11_OVERRIDE_PURE(ResultType, ArcFileInterface, modifyArcFile);
    }
};

class PyGeneralInterface : public GeneralInterface {
public:
    using GeneralInterface::GeneralInterface;

    void loadFiles(const QString &root, GameInstance &gameInstance, const ModListType &modList) override {
        PYBIND11_OVERRIDE_PURE(void, GeneralInterface, loadFiles, root, gameInstance, modList);
    }
    void saveFiles(const QString &root, GameInstance &gameInstance, const ModListType &modList) override {
        PYBIND11_OVERRIDE_PURE(void, GeneralInterface, saveFiles, root, gameInstance, modList);
    }
};

class PyUiMessageInterface : public UiMessageInterface {
private:
    typedef QMap<QString, LoadMessagesFunction> LoadResultType;
    typedef QMap<QString, SaveMessagesFunction> SaveResultType;
public:
    using UiMessageInterface::UiMessageInterface;

    QMap<QString, LoadMessagesFunction> loadUiMessages() override {
        PYBIND11_OVERRIDE_PURE(LoadResultType, UiMessageInterface, loadUiMessages);
    }
    QMap<QString, SaveMessagesFunction> freeUiMessages() override {
        PYBIND11_OVERRIDE_PURE(SaveResultType, UiMessageInterface, freeUiMessages);
    }
    void allocateUiMessages(const QString &root, GameInstance &gameInstance, const ModListType &modList) override {
        PYBIND11_OVERRIDE_PURE(void, UiMessageInterface, allocateUiMessages, root, gameInstance, modList);
    }
    QMap<QString, SaveMessagesFunction> saveUiMessages() override {
        PYBIND11_OVERRIDE_PURE(SaveResultType, UiMessageInterface, saveUiMessages);
    }
};

}

PYBIND11_EMBEDDED_MODULE(pycsmm, m) {
    pybind11::enum_<RuleSet>(m, "RuleSet", R"pycsmmdoc(
    Enum representing whether the board is easy or standard mode.
)pycsmmdoc")
            .value("Easy", Easy)
            .value("Standard", Standard);

    pybind11::enum_<BoardTheme>(m, "BoardTheme", R"pycsmmdoc(
    Enum representing the board's franchise theme.
)pycsmmdoc")
            .value("DragonQuest", DragonQuest)
            .value("Mario", Mario);

    pybind11::enum_<BgmId> bgmIdEnum(m, "BgmId", R"pycsmmdoc(
    Enum representing the background music id.
)pycsmmdoc");
    auto &bgmIdMap = Bgm::bgmIdMapping();
    for (auto it=bgmIdMap.begin(); it!=bgmIdMap.end(); ++it) {
        bgmIdEnum.value(it.key().toUtf8(), it.value());
    }

    pybind11::enum_<MusicType> musicTypeEnum(m, "MusicType",  R"pycsmmdoc(
    Enum representing the usage of the music.
)pycsmmdoc");
    auto &musicTypeMap = Music::stringToMusicTypeMapping();
    for (auto it=musicTypeMap.begin(); it!=musicTypeMap.end(); ++it) {
        musicTypeEnum.value(it.key().toUtf8(), it.value());
    }

    pybind11::enum_<Character> characterEnum(m, "Character", R"pycsmmdoc(
    Enum representing the character.
)pycsmmdoc");
    auto &charactersMap = stringToTourCharactersMapping();
    for (auto it = charactersMap.begin(); it != charactersMap.end(); ++it) {
        characterEnum.value(it.key().toUtf8(), it.value());
    }

    pybind11::enum_<LoopingMode>(m, "LoopingMode", R"pycsmmdoc(
    Enum representing how the board loops.
)pycsmmdoc")
            .value("None", None)
            .value("Vertical", Vertical)
            .value("Both", Both);

    pybind11::class_<OriginPoint>(m, "OriginPoint", R"pycsmmdoc(
    An ordered pair (x,y) representing switch rotation origin point.
)pycsmmdoc")
            .def(pybind11::init([](float x, float y) { return OriginPoint{x, y}; }), pybind11::arg("x"),  pybind11::arg("y"), R"pycsmmdoc(
    Constructor.
)pycsmmdoc")
            .def_readwrite("x", &OriginPoint::x, R"pycsmmdoc(
    The x-value.
)pycsmmdoc")
            .def_readwrite("y", &OriginPoint::y, R"pycsmmdoc(
    The y-value.
)pycsmmdoc");

    pybind11::class_<MusicEntry>(m, "MusicEntry", R"pycsmmdoc(
    Represents a music entry in the music BRSAR file.
)pycsmmdoc")
            .def(pybind11::init([](const QString &brstmBaseFilename, quint8 volume, qint32 brsarIndex, quint32 brstmFileSize) {
                return MusicEntry{brstmBaseFilename, volume, brsarIndex, brstmFileSize};
            }), pybind11::arg("brstmBaseFilename"), pybind11::arg("volume"), pybind11::arg("brsarIndex"), pybind11::arg("brstmFileSize"), R"pycsmmdoc(
    Constructor.
)pycsmmdoc")
            .def_readwrite("brstmBaseFilename", &MusicEntry::brstmBaseFilename, R"pycsmmdoc(
    The base filename of the music file.
)pycsmmdoc")
            .def_readwrite("volume", &MusicEntry::volume, R"pycsmmdoc(
    The volume from 0 to 100.
)pycsmmdoc")
            .def_readwrite("brsarIndex", &MusicEntry::brsarIndex, R"pycsmmdoc(
    The relative location of the music entry in the BRSAR file.
)pycsmmdoc")
            .def_readwrite("brstmFileSize", &MusicEntry::brstmFileSize, R"pycsmmdoc(
    The size of the BRSTM music file.
)pycsmmdoc");

    bindStdArray<bool, 128>(m, "VentureCardTable", R"pycsmmdoc(
    A boolean array-like type such that venture card x is enabled iff VentureCardTable[x] is enabled.
)pycsmmdoc");

    bindStdArray<QString, 4>(m, "FrbFiles", R"pycsmmdoc(
    An array-like type containing the frb file names.
)pycsmmdoc");

    pybind11::bind_vector<std::vector<OriginPoint>>(m, "OriginPoints", R"pycsmmdoc(
    A list-like type of switch rotation origins.
)pycsmmdoc");

    pybind11::bind_map<std::map<MusicType, MusicEntry>>(m, "MusicEntryTable", R"pycsmmdoc(
    A dict-like type mapping MusicTypes to MusicEntries.
)pycsmmdoc");

    bindStdArray<Character, 3>(m, "TourCharacters", R"pycsmmdoc(
    An array-like type containing the three tour opponents.
)pycsmmdoc");

    pybind11::bind_map<std::map<QString, QString>>(m, "LanguageTable", R"pycsmmdoc(
    Maps the language code to the localized name, string, etc.
)pycsmmdoc");

    pybind11::bind_vector<std::vector<QString>>(m, "StringList", R"pycsmmdoc(
    A list-like type of strings.
)pycsmmdoc");

    pybind11::bind_map<std::map<QString, std::vector<QString>>>(m, "ListLanguageTable", R"pycsmmdoc(
    Maps the language code to a localized list of localized names, strings, etc.
)pycsmmdoc");

    pybind11::bind_vector<std::vector<quint32>>(m, "LanguageIdList", R"pycsmmdoc(
    A list-like type of localization IDs.
)pycsmmdoc");

    pybind11::bind_vector<std::vector<MapDescriptor>>(m, "MapDescriptorList", R"pycsmmdoc(
    A list-like type of map descriptors.
)pycsmmdoc");

    pybind11::class_<MapDescriptor>(m, "MapDescriptor", R"pycsmmdoc(
    Stores relevant information for a Fortune Street board.
)pycsmmdoc")
            .def(pybind11::init<>())
            .def_readwrite("mapSet", &MapDescriptor::mapSet)
            .def_readwrite("zone", &MapDescriptor::zone)
            .def_readwrite("order", &MapDescriptor::order)
            .def_readwrite("isPracticeBoard", &MapDescriptor::isPracticeBoard)
            .def_readwrite("unlockId", &MapDescriptor::unlockId)
            .def_readwrite("ruleSet", &MapDescriptor::ruleSet)
            .def_readwrite("initialCash", &MapDescriptor::initialCash)
            .def_readwrite("targetAmount", &MapDescriptor::targetAmount)
            .def_readwrite("baseSalary", &MapDescriptor::baseSalary)
            .def_readwrite("salaryIncrement", &MapDescriptor::salaryIncrement)
            .def_readwrite("maxDiceRoll", &MapDescriptor::maxDiceRoll)
            .def_readwrite("ventureCards", &MapDescriptor::ventureCards)
            .def_readwrite("frbFiles", &MapDescriptor::frbFiles)
            .def_readwrite("switchRotationOrigins", &MapDescriptor::switchRotationOrigins)
            .def_readwrite("theme", &MapDescriptor::theme)
            .def_readwrite("background", &MapDescriptor::background)
            .def_readwrite("bgmId", &MapDescriptor::bgmId)
            .def_readwrite("mapIcon", &MapDescriptor::mapIcon)
            .def_readwrite("music", &MapDescriptor::music)
            .def_readwrite("loopingMode", &MapDescriptor::loopingMode)
            .def_readwrite("loopingModeRadius", &MapDescriptor::loopingModeRadius)
            .def_readwrite("loopingModeHorizontalPadding", &MapDescriptor::loopingModeHorizontalPadding)
            .def_readwrite("loopingModeVerticalSquareCount", &MapDescriptor::loopingModeVerticalSquareCount)
            .def_readwrite("tourBankruptcyLimit", &MapDescriptor::tourBankruptcyLimit)
            .def_readwrite("tourInitialCash", &MapDescriptor::tourInitialCash)
            .def_readwrite("tourCharacters", &MapDescriptor::tourCharacters)
            .def_readwrite("tourClearRank", &MapDescriptor::tourClearRank)
            .def_readwrite("nameMsgId", &MapDescriptor::nameMsgId)
            .def_readwrite("descMsgId", &MapDescriptor::descMsgId)
            .def_readwrite("names", &MapDescriptor::names)
            .def_readwrite("descs", &MapDescriptor::descs)
            .def_readwrite("internalName", &MapDescriptor::internalName)
            .def_readwrite("mapDescriptorFilePath", &MapDescriptor::mapDescriptorFilePath)
            .def_readwrite("districtNames", &MapDescriptor::districtNames)
            .def_readwrite("districtNameIds", &MapDescriptor::districtNameIds)
            .def_readwrite("extraData", &MapDescriptor::extraData);

    pybind11::class_<AddressMapper>(m, "AddressMapper")
            .def("canConvertToFileAddress", &AddressMapper::canConvertToFileAddress)
            .def("toFileAddress", &AddressMapper::toFileAddress)
            .def("boomToFileAddress", &AddressMapper::boomToFileAddress)
            .def("fileAddressToStandardVirtualAddress", &AddressMapper::fileAddressToStandardVirtualAddress)
            .def("boomStreetToStandard", &AddressMapper::boomStreetToStandard)
            .def("standardToBoomStreet", &AddressMapper::standardToBoomStreet)
            .def("canConvertBoomStreetToStandard", &AddressMapper::canConvertBoomStreetToStandard);

    pybind11::class_<FreeSpaceManager>(m, "FreeSpaceManager")
            .def("addFreeSpace", &FreeSpaceManager::addFreeSpace)
            .def("calculateTotalRemainingFreeSpace", &FreeSpaceManager::calculateTotalRemainingFreeSpace)
            .def("calculateTotalFreeSpace", &FreeSpaceManager::calculateTotalFreeSpace)
            .def("calculateLargestFreeSpaceBlockSize", &FreeSpaceManager::calculateLargestFreeSpaceBlockSize)
            .def("calculateLargestRemainingFreeSpaceBlockSize", &FreeSpaceManager::calculateLargestRemainingFreeSpaceBlockSize)
            .def("allocateUnusedSpace", [](FreeSpaceManager &fsm, const QByteArray &bytes, pybind11::object fileObj, const AddressMapper &fileMapper, const QString &purpose, bool reuse) {
                PyQIODevice device(fileObj);
                QDataStream stream(&device);
                fsm.allocateUnusedSpace(bytes, stream, fileMapper, purpose, reuse);
            })
            .def("reset", &FreeSpaceManager::reset);

    pybind11::class_<GameInstance>(m, "GameInstance")
            .def_property("mapDescriptors",
                          qNonConstOverload<>(&GameInstance::mapDescriptors),
                          [](GameInstance &instance, const std::vector<MapDescriptor> &descs) { instance.mapDescriptors() = descs; })
            .def("addressMapper", &GameInstance::addressMapper)
            .def("freeSpaceManager", qNonConstOverload<>(&GameInstance::freeSpaceManager))
            .def("nextUiMessageId", &GameInstance::nextUiMessageId);

    pybind11::class_<CSMMMod, PyCSMMMod, std::shared_ptr<CSMMMod>>(m, "CSMMMod")
            .def(pybind11::init<>())
            .def("modId", &CSMMMod::modId)
            .def("priority", &CSMMMod::priority)
            .def("depends", &CSMMMod::depends)
            .def("after", &CSMMMod::after)
            .def("modpackDir", &CSMMMod::modpackDir);

    pybind11::class_<ArcFileInterface, PyArcFileInterface, std::shared_ptr<ArcFileInterface>>(m, "ArcFileInterface")
            .def(pybind11::init<>())
            .def("modifyArcFile", &ArcFileInterface::modifyArcFile);

    pybind11::class_<GeneralInterface, PyGeneralInterface, std::shared_ptr<GeneralInterface>>(m, "GeneralInterface")
            .def(pybind11::init<>())
            .def("loadFiles", &GeneralInterface::loadFiles)
            .def("saveFiles", &GeneralInterface::saveFiles);

    pybind11::class_<UiMessageInterface, PyUiMessageInterface, std::shared_ptr<UiMessageInterface>>(m, "UiMessageInterface")
            .def(pybind11::init<>())
            .def("loadUiMessages", &UiMessageInterface::loadUiMessages)
            .def("saveUiMessages", &UiMessageInterface::saveUiMessages)
            .def("freeUiMessages", &UiMessageInterface::freeUiMessages)
            .def("allocateUiMessages", &UiMessageInterface::allocateUiMessages);
}
