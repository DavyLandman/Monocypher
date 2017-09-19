#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "monocypher.h"
#include "sha512.h"
#include "utils.h"

typedef struct timespec timespec;

// TODO: provide a user defined buffer size
#define KILOBYTE 1024
#define MEGABYTE (1024 * KILOBYTE)
#define SIZE     MEGABYTE
#define DIVIDER  (MEGABYTE / SIZE)

timespec diff(timespec start, timespec end)
{
    timespec duration;
    duration.tv_sec  = end.tv_sec  - start.tv_sec;
    duration.tv_nsec = end.tv_nsec - start.tv_nsec;
    if (duration.tv_nsec < 0) {
        duration.tv_nsec += 1000000000;
        duration.tv_sec  -= 1;
    }
    return duration;
}

timespec min(timespec a, timespec b)
{
    if (a.tv_sec < b.tv_sec ||
        (a.tv_sec == b.tv_sec && a.tv_nsec < b.tv_nsec)) {
        return a;
    }
    return b;
}

u64 speed(timespec duration)
{
    static const u64 giga = 1000000000;
    return giga / (duration.tv_nsec + duration.tv_sec * giga);
}

static void print(const char *name, u64 speed, const char *unit)
{
    printf("%s: %4" PRIu64 " %s\n", name, speed, unit);
}

// TODO: adjust this crap
#define TIMESTAMP(t)                            \
    timespec t;                                 \
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t)

#define TIMING_START                            \
    timespec duration;                          \
    duration.tv_sec  = 3600 * 24;               \
    duration.tv_nsec = 0;                       \
    FOR (i, 0, 50) {                            \
        TIMESTAMP(start);

#define TIMING_END                              \
    TIMESTAMP(end);                             \
    duration = diff(start, end);                \
    } /* end FOR*/                              \
    return speed(duration)


static u64 chacha20(void)
{
    static u8  in    [SIZE];  p_random(in   , SIZE);
    static u8  key   [  32];  p_random(key  ,   32);
    static u8  nonce [   8];  p_random(nonce,    8);
    static u8  out  [SIZE];

    TIMING_START {
        crypto_chacha_ctx ctx;
        crypto_chacha20_init(&ctx, key, nonce);
        crypto_chacha20_encrypt(&ctx, out, in, SIZE);
    }
    TIMING_END;
}

static u64 poly1305(void)
{
    static u8  in    [SIZE];  p_random(in   , SIZE);
    static u8  key   [  32];  p_random(key  ,   32);
    static u8  out  [  16];

    TIMING_START {
        crypto_poly1305_auth(out, in, SIZE, key);
    }
    TIMING_END;
}

static u64 authenticated(void)
{
    static u8  in    [SIZE];  p_random(in   , SIZE);
    static u8  key   [  32];  p_random(key  ,   32);
    static u8  nonce [   8];  p_random(nonce,    8);
    static u8  out   [SIZE];
    static u8  mac   [  16];

    TIMING_START {
        crypto_lock(mac, out, key, nonce, in, SIZE);
    }
    TIMING_END;
}

static u64 blake2b(void)
{
    static u8 in  [SIZE];  p_random(in , SIZE);
    static u8 key [  32];  p_random(key,   32);
    static u8 hash[  64];

    TIMING_START {
        crypto_blake2b_general(hash, 64, key, 32, in, SIZE);
    }
    TIMING_END;
}

static u64 argon2i(void)
{
    size_t    nb_blocks = 1024;
    static u8 work_area[MEGABYTE];
    static u8 password [      16];  p_random(password, 16);
    static u8 salt     [      16];  p_random(salt    , 16);
    static u8 hash     [      32];

    TIMING_START {
        crypto_argon2i(hash, 32, work_area, nb_blocks, 3,
                       password, 16, salt, 16, 0, 0, 0, 0);
    }
    TIMING_END;
}

static u64 x25519(void)
{
    u8 in  [32] = {9};
    u8 out [32] = {9};

    TIMING_START {
        if (crypto_x25519(out, out, in)) {
            printf("Monocypher x25519 rejected public key\n");
        }
    }
    TIMING_END;
}

static u64 edDSA_sign(void)
{
    u8 sk         [32];  p_random(sk, 32);
    u8 pk         [32];  crypto_sign_public_key(pk, sk);
    u8 message    [64];  p_random(message, 64);
    u8 signature  [64];

    TIMING_START {
        crypto_sign(signature, sk, pk, message, 64);
    }
    TIMING_END;
}

static u64 edDSA_check(void)
{
    u8 sk         [32];  p_random(sk, 32);
    u8 pk         [32];  crypto_sign_public_key(pk, sk);
    u8 message    [64];  p_random(message, 64);
    u8 signature  [64];

    crypto_sign(signature, sk, pk, message, 64);

    TIMING_START {
        if (crypto_check(signature, pk, message, 64)) {
            printf("Monocypher verification failed\n");
        }
    }
    TIMING_END;
}

int main()
{
    print("Chacha20         ", chacha20()      / DIVIDER, "Mb/s"       );
    print("Poly1305         ", poly1305()      / DIVIDER, "Mb/s"       );
    print("Auth'd encryption", authenticated() / DIVIDER, "Mb/s"       );
    print("Blake2b          ", blake2b()       / DIVIDER, "Mb/s"       );
    print("Argon2i          ", argon2i()      , "Mb/s (3 passes)"      );
    print("x25519           ", x25519()       , "exchanges  per second");
    print("EdDSA(sign)      ", edDSA_sign()   , "signatures per second");
    print("EdDSA(check)     ", edDSA_check()  , "checks     per second");
    printf("\n");
    return 0;
}
