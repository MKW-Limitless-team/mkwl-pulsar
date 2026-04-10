#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <Network/RSA.hpp>
#include <Network/SHA256.hpp>
#include <core/rvl/DWC/DWC.hpp>
#include <core/rvl/NHTTP/NHTTP.hpp>
#include <core/rvl/ipc/ipc.hpp>
#include <kamek.hpp>

#define WWFC_PRODUCTION 1

#include <Network/WiiLink/wwfcError.h>
#include <Network/WiiLink/wwfcPublicKey.h>
#include <Network/WiiLink/wwfcTypes.h>

static u8 s_payloadBlock[ WWFC_PAYLOAD_BLOCK_SIZE + 0x20 ];
static void *s_payload = nullptr;
static bool s_payloadReady = false;
static u8 s_saltHash[ SHA256_DIGEST_SIZE ];

extern "C"
{
void Real_DWCi_Auth_SendRequest( int param_1, int param_2, int param_3, int param_4, int param_5,
        int param_6 );

extern s32 *s_auth_work;
extern s32 s_auth_error;
}

static asm void DWCi_Auth_SendRequest( int param_1, int param_2, int param_3, int param_4,
        int param_5, int param_6 )
{
    // clang-format off
    nofralloc

    stwu r1, -0x1B0(r1)
    b Real_DWCi_Auth_SendRequest
    // clang-format on
}

bool GenerateRandomSalt( u8 *out, u32 *deviceId )
{
    // Generate cryptographic random with ES_Sign
    s32 fd = IOS::Open( "/dev/es", IOS::MODE_NONE );
    if( fd < 0 )
    {
        return false;
    }

    __declspec( align( 0x40 ) ) u8 dummy[ 0x20 ];
    dummy[ 0 ] = 0x7a;
    __declspec( align( 0x40 ) ) u8 eccCert[ 0x180 ];
    __declspec( align( 0x40 ) ) u8 eccSignature[ 0x3C ];

    __declspec( align( 0x20 ) ) IOS::IOCtlvRequest vec[ 3 ];
    vec[ 0 ].address = &dummy;
    vec[ 0 ].size = 1;
    vec[ 1 ].address = eccSignature;
    vec[ 1 ].size = 0x3C;
    vec[ 2 ].address = eccCert;
    vec[ 2 ].size = 0x180;

    // ES_Sign
    s32 ret = IOS::IOCtlv( fd, IOS::IOCtlType( 0x30 ), 1, 2, vec );
    IOS::Close( fd );

    if( ret < 0 )
    {
        return false;
    }

    SHA256Context ctx;
    SHA256Init( &ctx );
    SHA256Update( &ctx, eccSignature, 0x3C );
    SHA256Update( &ctx, eccCert, 0x180 );
    memcpy( out, SHA256Final( &ctx ), SHA256_DIGEST_SIZE );

    *deviceId = strtoul( reinterpret_cast< char * >( eccCert ) + 0xC4 + 2, nullptr, 16 );

    return true;
}

s32 HandleResponse( u8 *block )
{
    register wwfc_payload *__restrict payload = reinterpret_cast< wwfc_payload * >( block );

    if( *reinterpret_cast< u32 * >( payload ) != 0x57574643 /* WWFC */ )
    {
        return WL_ERROR_PAYLOAD_STAGE1_HEADER_CHECK;
    }

    if( payload->header.total_size < sizeof( wwfc_payload ) ||
            payload->header.total_size > WWFC_PAYLOAD_BLOCK_SIZE )
    {
        return WL_ERROR_PAYLOAD_STAGE1_LENGTH_ERROR;
    }

    if( memcmp( payload->salt, s_saltHash, SHA256_DIGEST_SIZE ) != 0 )
    {
        return WL_ERROR_PAYLOAD_STAGE1_SALT_MISMATCH;
    }

    SHA256Context ctx;
    SHA256Init( &ctx );
    SHA256Update( &ctx, reinterpret_cast< u8 * >( payload ) + sizeof( wwfc_payload_header ),
            payload->header.total_size - sizeof( wwfc_payload_header ) );
    u8 *hash = SHA256Final( &ctx );

    if( !RSAVerify( reinterpret_cast< const RSAPublicKey * >( wwfc_payload_public_key ),
                payload->header.signature, hash ) )
    {
        return WL_ERROR_PAYLOAD_STAGE1_SIGNATURE_INVALID;
    }

    // Flush data cache and invalidate instruction cache
    for( register u32 i = 0; i < 0x20000; i += 0x20 )
    {
        asm( dcbf i, payload; sync; icbi i, payload; isync; );
    }
    asm( sc );

#if 0
    // Disable unnecessary patches
    u32 patchMask = WWFC_PATCH_LEVEL_CRITICAL | WWFC_PATCH_LEVEL_BUGFIX | WWFC_PATCH_LEVEL_SUPPORT;
    for( wwfc_patch *patch = reinterpret_cast< wwfc_patch * >(
                            block + payload->info.patch_list_offset ),
                    *end = reinterpret_cast< wwfc_patch * >( block + payload->info.patch_list_end );
            patch < end; patch++ )
    {
        if( patch->level == WWFC_PATCH_LEVEL_CRITICAL || ( patch->level & patchMask ) )
        {
            continue;
        }

        // Otherwise disable the patch
        patch->level |= WWFC_PATCH_LEVEL_DISABLED;
    }
#endif

    wwfc_payload_entry_t entry = reinterpret_cast< wwfc_payload_entry_t >(
            reinterpret_cast< u8 * >( payload ) + payload->info.entry_point );

    s32 result = entry( payload );

    wwfc_function_exec_t functionExec =
            reinterpret_cast< wwfc_function_exec_t >( payload->info.function_exec );

    functionExec( WWFC_FUNCTION_SET_VALUE, WWFC_KEY_ENABLE_AGGRESSIVE_PACKET_CHECKS,
            WWFC_BOOLEAN_FALSE );

    return result;
}

void OnPayloadReceived( s32 result, void *response, void *userdata )
{
    if( response == nullptr )
    {
        return;
    }

    NHTTP::DestroyResponse( response );

    if( result != 0 )
    {
        return;
    }

    s32 error = HandleResponse( reinterpret_cast< u8 * >( s_payload ) );
    if( error != 0 )
    {
        s_auth_error = error;
        return;
    }

    s_payloadReady = true;
    s_auth_error = -1; // This error code will retry auth
}

void DWCi_Auth_SendRequest_Patched( int param_1, int param_2, int param_3, int param_4, int param_5,
        int param_6 )
{
    if( s_payloadReady )
    {
        DWCi_Auth_SendRequest( param_1, param_2, param_3, param_4, param_5, param_6 );
        return;
    }

    s_payload = (void *)( ( u32( s_payloadBlock ) + 31 ) & ~31 );
    memset( s_payload, 0, WWFC_PAYLOAD_BLOCK_SIZE );

    u8 salt[ SHA256_DIGEST_SIZE ];
    u32 deviceId;
    if( !GenerateRandomSalt( salt, &deviceId ) )
    {
        s_auth_error = WL_ERROR_PAYLOAD_STAGE1_MAKE_REQUEST;
    }

    static const char *hexConv = "0123456789abcdef";
    char saltHex[ SHA256_DIGEST_SIZE * 2 + 1 ];
    for( int i = 0; i < SHA256_DIGEST_SIZE; i++ )
    {
        saltHex[ i * 2 ] = hexConv[ salt[ i ] >> 4 ];
        saltHex[ i * 2 + 1 ] = hexConv[ salt[ i ] & 0xf ];
    }
    saltHex[ SHA256_DIGEST_SIZE * 2 ] = 0;

    char uri[ 0x100 ];
    sprintf( uri, "payload?c=pulsar2&d=%08x&g=RMC%cD00&s=%s", deviceId, *(char *)0x80003183,
            saltHex );

    // Generate salt hash
    SHA256Context ctx;
    SHA256Init( &ctx );
    SHA256Update( &ctx, uri, strlen( uri ) );
    memcpy( s_saltHash, SHA256Final( &ctx ), SHA256_DIGEST_SIZE );

    char url[ 0x100 ];
    sprintf( url, "http://nas.%s/%s&h=%02x%02x%02x%02x", WWFC_DOMAIN, uri, s_saltHash[ 0 ],
            s_saltHash[ 1 ], s_saltHash[ 2 ], s_saltHash[ 3 ] );

    void *request = NHTTP::CreateRequest( url, 0, s_payload, WWFC_PAYLOAD_BLOCK_SIZE,
            reinterpret_cast< void * >( OnPayloadReceived ), 0 );

    if( request == nullptr )
    {
        s_auth_error = WL_ERROR_PAYLOAD_STAGE1_MAKE_REQUEST;
        return;
    }

    s_auth_work[ 0x59E0 / 4 ] = NHTTP::SendRequestAsync( request );
}

kmBranch( 0x800ED6E8, DWCi_Auth_SendRequest_Patched );

void SetEnableAggressivePacketChecks( )
{
    RKNet::Controller &controller = *RKNet::Controller::sInstance;
    controller.connectionState = RKNet::CONNECTIONSTATE_ROOM;

    if( !s_payloadReady )
    {
        return;
    }

    wwfc_boolean_t enable = WWFC_BOOLEAN_FALSE;
    switch( controller.roomType )
    {
    case RKNet::ROOMTYPE_VS_WW:
    case RKNet::ROOMTYPE_BT_WW:
    case RKNet::ROOMTYPE_JOINING_WW:
    case RKNet::ROOMTYPE_JOINING_BT_WW:
        enable = WWFC_BOOLEAN_RESET;
    }

    wwfc_function_exec_t functionExec =
            reinterpret_cast< wwfc_payload_ex * >( s_payload )->info.function_exec;

    functionExec( WWFC_FUNCTION_SET_VALUE, WWFC_KEY_ENABLE_AGGRESSIVE_PACKET_CHECKS, enable );
}

kmCall( 0x806577F4, SetEnableAggressivePacketChecks );

// Enable DWC logging
kmWrite32( 0x803862C0, 0xFFFFFFFF );
