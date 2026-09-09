//Unused SD backend shim for port parity with rr-pulsar's LooseArchiveOverrides.
//This mod has no SD IO; keeping IOType_SD + this type makes the (never-exercised)
//SD override scan compile while returning nothing.

#ifndef _PULSAR_SDIO_
#define _PULSAR_SDIO_

#include <IO/IO.hpp>

namespace Pulsar {

class SDIO : public IO {
public:
    SDIO(IOType type, EGG::Heap* heap, EGG::TaskThread* taskThread) : IO(type, heap, taskThread) {}
    bool OpenFile(const char* path, u32 mode) override { return false; }
    bool CreateAndOpen(const char* path, u32 mode) override { return false; }
    void GetCorrectPath(char* realPath, const char* path) const override { realPath[0] = '\0'; }
    bool RenameFile(const char* oldPath, const char* newPath) const override { return false; }
    bool FolderExists(const char* path) const override { return false; }
    bool CreateFolder(const char* path) override { return false; }
    void ReadFolder(const char* path) override { this->fileCount = 0; }

    //SD-specific methods used by rr's override scanner; always report none/fail
    bool OpenFolderStream(const char* path) { return false; }
    bool ReadFolderEntry(char* fileName, int size, bool& isDirectory) { return false; }
    void CloseFolderStream() {}
};

}//namespace Pulsar
#endif