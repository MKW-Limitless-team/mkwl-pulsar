#include <types.hpp>
#include <include/c_stdio.h>
#include <core/GS/GP/GPTypes.hpp>
#include <core/GS/GP/GPUtility.hpp>
#include <core/rvl/DWC/DWCMatch.hpp>

namespace Pulsar {
namespace Network {

//true while the GP output buffer is plausible Wii RAM with sane bounds (GP objects are freed on disconnect)
static bool IsGpConnectionAlive(GP::Connection** connection) {
    GP::IConnection* iconnection = reinterpret_cast<GP::IConnection*>(*connection);
    const u32 buffer = reinterpret_cast<u32>(iconnection->outputBuffer.buffer);
    const u32 region = buffer >> 28;
    if(buffer == 0 || (region != 0x8 && region != 0x9 && region != 0xC && region != 0xD)) return false;
    const GP::IBuffer& output = iconnection->outputBuffer;
    if(output.size <= 0 || output.len < 0 || output.len > output.size) return false;
    return true;
}

void Report(const char* key, const char* string) {
    if (DWC::MatchControl::sInstance == nullptr) return;
    GP::Connection** connection = DWC::MatchControl::sInstance->gpConnection;
    if (connection == nullptr || *connection == nullptr) return;
    if (!IsGpConnectionAlive(connection)) return;

    GP::IConnection* iconnection = reinterpret_cast<GP::IConnection*>(*connection);

    GP::gpiAppendStringToBuffer(
        connection, &iconnection->outputBuffer, "\\wl:report\\\\");
    GP::gpiAppendStringToBuffer(
        connection, &iconnection->outputBuffer, key);
    GP::gpiAppendStringToBuffer(
        connection, &iconnection->outputBuffer, "\\");
    GP::gpiAppendStringToBuffer(
        connection, &iconnection->outputBuffer, string);
    GP::gpiAppendStringToBuffer(
        connection, &iconnection->outputBuffer, "\\final\\");
}

void ReportU32(const char* key, u32 uint) {
    char buffer[sizeof("4294967295")];

    if (snprintf(buffer, sizeof(buffer), "%lu", uint) < 0) {
        return;
    }

    Report(key, buffer);
}

void PumpGPI()
{
    if (DWC::MatchControl::sInstance == nullptr) return;

    GP::Connection** connection = DWC::MatchControl::sInstance->gpConnection;
    if (connection == nullptr || *connection == nullptr) return;
    if (!IsGpConnectionAlive(connection)) return;

    // Non-blocking pump
    GP::gpiProcess(connection, 0);
}

}  // namespace Network
}  // namespace Pulsar
