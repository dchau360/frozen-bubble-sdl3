// GameplayRng must be replay-capturable: deterministic from a seed, and
// unaffected by unrelated libc rand()/srand() calls interleaved with it (the
// property cosmetic code -- menus, highscore pictures, transitions -- would
// otherwise threaten by sharing the one global rand() stream gameplay used
// to draw from). See src/gameplay_rng.h and docs/REPLAY_PROGRESS.md.

#include "gameplay_rng.h"

#include <cstdlib>
#include <iostream>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)

static std::vector<uint32_t> Draw(GameplayRng &rng, int count) {
    std::vector<uint32_t> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) out.push_back(rng.Next());
    return out;
}

int main() {
    // Same seed -> identical sequence, on two independent instances.
    {
        GameplayRng a, b;
        a.Seed(12345);
        b.Seed(12345);
        CHECK(Draw(a, 200) == Draw(b, 200));
    }

    // Different seeds -> sequences diverge (checked over many seed pairs so
    // one unlucky collision can't make the test flaky).
    {
        int matches = 0;
        for (uint32_t seed = 1; seed <= 50; ++seed) {
            GameplayRng a, b;
            a.Seed(seed);
            b.Seed(seed + 1);
            if (Draw(a, 20) == Draw(b, 20)) ++matches;
        }
        CHECK(matches == 0);
    }

    // Range(a,b) always stays within [a,b], including a==b and a small span
    // that does not evenly divide the generator's output range.
    {
        GameplayRng rng;
        rng.Seed(999);
        for (int i = 0; i < 5000; ++i) {
            int v = rng.Range(0, 6);
            CHECK(v >= 0 && v <= 6);
        }
        for (int i = 0; i < 100; ++i) {
            CHECK(rng.Range(4, 4) == 4);
        }
        for (int i = 0; i < 2000; ++i) {
            float f = rng.Range(4.0f);
            CHECK(f >= 0.0f && f < 4.0f);
        }
    }

    // The actual property this type exists for: interleaving unrelated
    // libc rand()/srand() calls between draws must not perturb the sequence.
    {
        GameplayRng isolated, reference;
        isolated.Seed(42);
        reference.Seed(42);

        std::vector<uint32_t> isolatedOut, referenceOut;
        for (int i = 0; i < 100; ++i) {
            // Simulate cosmetic code drawing from (and reseeding) the global
            // stream in between gameplay draws.
            std::srand(static_cast<unsigned>(i) * 7919u);
            for (int j = 0; j < 3; ++j) std::rand();
            isolatedOut.push_back(isolated.Next());
        }
        for (int i = 0; i < 100; ++i) referenceOut.push_back(reference.Next());
        CHECK(isolatedOut == referenceOut);
    }

    std::cout << "gameplay rng tests passed\n";
    return 0;
}
