# 🤖 AI Coding Workflow - How I (the LLM) Should Work

## Problem Statement

**Before** :
- I code blindly
- User compiles
- User tests manually
- Tells me "it works" or "it doesn't"
- Slow feedback loop (minutes to hours)
- High chance of regression

**After (with this system)** :
- I can test my code immediately
- Automated verification
- Fast feedback loop (seconds)
- Low regression risk
- Higher quality code

---

## My New Workflow

### Step 1 : BASELINE (Before Touching Anything)

```bash
# Run tests to establish baseline
./run_tests.sh

# Expected output:
# ✅ Some tests pass
# ❌ Some tests fail (known bugs)

# Document current state:
# - Which tests pass?
# - Which tests fail?
# - Why do they fail?
```

**I should** :
- Read the test output carefully
- Understand which tests are already passing
- Understand which bugs exist
- NOT break passing tests

---

### Step 2 : PICK ONE BUG

```
From BUG_ANALYSIS.md, choose:
Priority 1 : TABLE EMBED
Priority 2 : WEB EMBED  
Priority 3 : HIGHLIGHT

ONE bug at a time.
```

**Why** : Multiple bugs = multiple variables = hard to debug.

---

### Step 3 : UNDERSTAND THE BUG

**Read** :
1. Bug description in BUG_ANALYSIS.md
2. Related code (file + line numbers given)
3. Existing test (test_runner.c)

**Identify** :
- What SHOULD happen?
- What ACTUALLY happens?
- Where is the code that's broken?

**Write down** (in my response to user) :
```
Bug: TABLE EMBED doesn't render

Expected:
- Table should show preview in note
- Grid data should be visible

Actual:
- Blank space
- grid pointer is NULL

Root cause:
- AslanTableViewBlock->source_page_id not set
- Grid page not found during render

Files to fix:
- src/core/engine.c (allocation)
- src/platform/ui_host.c (rendering)
```

---

### Step 4 : WRITE THE FIX

**Propose code change** :
```c
// In engine.c, line XXX
// OLD CODE:
AslanTableViewBlock *tvb = calloc(1, sizeof(AslanTableViewBlock));
r->block = tvb;

// NEW CODE:
AslanTableViewBlock *tvb = calloc(1, sizeof(AslanTableViewBlock));
tvb->source_page_id = target_page_id;  // ← ADD THIS
tvb->collapsed = 0;                     // ← ADD THIS
r->block = tvb;
```

**Explain** :
- What I changed
- Why I changed it
- What should happen now

---

### Step 5 : VERIFY (Critical!)

**Ask user to** :
```bash
# Compile
cd build && make

# Run tests
cd .. && ./run_tests.sh

# Expected:
# ✅ test_table_embed() should still pass (structure)
# ✅ No regression (other tests still pass)
```

**Then ask** :
```
Please test manually:
1. Launch Aslan
2. Create a Grid page
3. Create a Note page
4. Insert table embed
5. Does it show now?

Expected: Table preview visible
```

---

### Step 6 : ITERATE

**If tests fail** :
- Read error message
- Understand what broke
- Fix it
- Test again

**If visual test fails** (user says "still doesn't work") :
- Ask for details : "What do you see?"
- Ask for specifics : "Blank? Crash? Wrong size?"
- Adjust code
- Test again

**If everything works** :
- Move to next bug
- Repeat process

---

## Test-Driven Approach

### Option A : Fix Existing Test

```c
// Test currently passes on structure, fails on rendering

// BEFORE:
test_table_embed() {
    // ... creates structure ...
    ASSERT(note->rows[0].block != NULL);  // ✅ Passes
    // But rendering is not tested
}

// ADD:
test_table_embed() {
    // ... creates structure ...
    ASSERT(note->rows[0].block != NULL);  // ✅ Passes
    
    // NEW: Verify block is properly initialized
    AslanTableViewBlock *tvb = (AslanTableViewBlock *)note->rows[0].block;
    ASSERT_EQ(tvb->source_page_id, table_page);  // ← Should pass after fix
}
```

### Option B : Add New Test

```c
// If a bug has no test, ADD ONE

void test_table_embed_rendering() {
    TEST("Table Embed Renders with Valid Data");
    
    // Setup: Create table with data
    // Setup: Create note with embed
    // Setup: Set source_page_id correctly
    
    // Verify: Block has correct source_page_id
    // Verify: Grid can be found
    // Verify: Grid has data
    
    test_passed_inc();
}
```

---

## Things I CAN Test (without GUI)

```
✅ Data structures (correct allocation)
✅ Pointers (not NULL)
✅ IDs (correct values)
✅ Strings (correct content)
✅ Logic (correct flow)
✅ Edge cases (NULL handling, empty data)
```

## Things I CANNOT Test (need user)

```
❌ Visual rendering (need eyes)
❌ Colors (need screenshot)
❌ Positioning (need ruler)
❌ Input handling (need keyboard/mouse)
❌ Performance (need timing)
```

**Solution** : After automated tests pass, ask user for visual confirmation.

---

## Communication Template

### When Proposing a Fix

```markdown
## Bug Fix: [BUG NAME]

### Analysis
[What I found wrong]

### Proposed Changes
[File, line, code change]

### Testing
1. Automated:
   - Run ./run_tests.sh
   - Expect test_XXX to pass

2. Manual:
   - [Step-by-step test procedure]
   - Expected result: [What should happen]

### Risk
- Low : Only touches bug area
- Medium : Touches related code
- High : Changes core logic

### Next Steps
1. User compiles
2. User runs tests
3. User confirms visual
4. Move to next bug
```

---

## Red Flags (When to STOP)

```
🚩 Tests that were passing now fail
   → STOP, revert, understand why

🚩 Code change touches >3 files
   → STOP, refactor into smaller changes

🚩 User says "it's worse now"
   → STOP, revert, re-analyze

🚩 I don't understand the code
   → STOP, ask user for clarification

🚩 Fix requires changing architecture
   → STOP, discuss with user first
```

---

## Success Metrics

**Per Bug** :
```
Before: ❌ Test fails
After:  ✅ Test passes
User:   ✅ Visual confirmation

= Bug FIXED
```

**Overall** :
```
Week 1:  7/10 tests pass (baseline)
Week 2:  8/10 tests pass (+1 bug fixed)
Week 3:  9/10 tests pass (+1 bug fixed)
Week 4: 10/10 tests pass (ALL BUGS FIXED)

= Phase 1 COMPLETE
```

---

## Example Session

```
ME (LLM):
"Running baseline tests..."
[reads run_tests.sh output]
"Currently 7/10 tests pass. 3 bugs to fix."

"Picking Bug #1: TABLE EMBED"
[reads BUG_ANALYSIS.md]
[reads engine.c code]
[reads ui_host.c code]

"Root cause: source_page_id not initialized"
"Fix: Add tvb->source_page_id = target_id in engine.c line 1093"

[proposes code change]

"Please:
1. Apply this change
2. Run ./run_tests.sh
3. Test manually with Grid embed
4. Report back"

USER:
"Tests pass, visual looks good!"

ME:
"✅ Bug #1 FIXED. Moving to Bug #2..."
```

---

## Philosophy

**Antirez quote** (paraphrased) :
> "Code without tests is guesswork.
> Tests without automation is faith.
> Automation without feedback is hope.
> We don't code on hope."

**My philosophy** :
- Test before changing
- Test after changing
- One thing at a time
- Never break working code
- Fast feedback > slow perfection

---

## Summary Checklist

Before proposing ANY code change:

```
□ I ran ./run_tests.sh (baseline)
□ I identified ONE specific bug
□ I read the relevant code
□ I understand the root cause
□ My fix is minimal (<50 lines)
□ My fix is testable
□ I explained what I'm doing
□ I provided test steps
```

After user applies change:

```
□ User ran ./run_tests.sh
□ Tests still pass (no regression)
□ User tested visually
□ Bug is confirmed fixed
□ Ready for next bug
```

---

**Next Action** : User should run `./run_tests.sh` to establish baseline.
