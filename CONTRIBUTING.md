# Contributing to EH3D-NODE-BUNGARD-AUX

Thank you for looking. This node watches a spindle and measures a table, so changes are small and carefully tested.

1. **Nothing that changes how a measure is made or when the protect output opens changes without proof.** A change in the measure (`core/laser.hpp`), in the watch of the spindle (`core/spindle.hpp`) or in the settings that decide a limit needs a test that covers it, including the refusals, and a note of what was observed on hardware once there is any.
2. **Every behaviour has a test**, including the negative ones: a value out of range, a reading that is odd, a sensor that fails, a request without the token.
3. **Before a pull request:** the host tests (`cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host`) and `python -I tools/ci_validate.py` must pass.
4. **The README files keep the same structure** in eight languages (English, Spanish, Italian, German, French, European Portuguese, Chinese and Japanese).
5. **Every tuned value is a setting** in `head.cfg`, with its default in `head.cfg.example`; a safety limit starts in its conservative position.
6. **No files of other people's software.** Do not add programs, manuals or demo code of other makers; the drivers are written for this repository or come from the component registry.
7. Commits are written in English, small and about one thing.
