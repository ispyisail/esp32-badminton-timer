# Quick Implementation Checklist

Quick reference for implementing the upgrade plan. See UPGRADE_PLAN.md for full details.

---

## Phase 1: Critical & High Priority (Master Branch)

### Sprint 1: Security (Day 1 - 2 hours)
- [ ] Move OTA password to wifi_credentials.h
- [ ] Implement WebSocket authentication
- [ ] Add server-side input validation
- [ ] Test authentication flow

### Sprint 2: WiFi Portal (Day 1 - 1.5 hours)
- [ ] Add Android captive portal handlers (/generate_204, /gen_204)
- [ ] Add iOS handlers (/hotspot-detect.html, etc.)
- [ ] Increase portal timeout to 300 seconds
- [ ] Replace std::vector with static array
- [ ] Test with Android and iOS devices

### Sprint 3: Timer Sync (Day 2 - 2.5 hours)
- [ ] Add periodic sync broadcast (server-side)
- [ ] Add serverMillis to sync message
- [ ] Replace setInterval with requestAnimationFrame (client-side)
- [ ] Implement elapsed time calculation
- [ ] Test for smooth countdown and no drift

### Sprint 4: Error Handling (Day 2 - 1 hour)
- [ ] Add SPIFFS failure restart
- [ ] Add sendError() function
- [ ] Add state validation in handleWebSocketMessage
- [ ] Implement exponential backoff reconnection
- [ ] Add integer overflow protection

### Sprint 5: Polish & Test (Day 3 - 1 hour)
- [ ] Add connection status indicator to UI
- [ ] Add JSON overflow checking
- [ ] Add WebSocket disconnect handler
- [ ] Add preferences error handling
- [ ] Run full test suite
- [ ] Document breaking changes
- [ ] Commit and push to master

**Phase 1 Verification:**
- [ ] Android connects to portal ✓
- [ ] Timer smooth with no jitter ✓
- [ ] Authentication works ✓
- [ ] 24-hour stability test ✓

---

## Phase 2: Enhancements (Feature Branch)

### Sprint 6: Refactoring (Day 4 - 3 hours)
- [ ] Create src/config.h with all constants
- [ ] Create src/timer.h + cpp
- [ ] Create src/websocket.h + cpp
- [ ] Create src/wifi_manager.h + cpp
- [ ] Create src/siren.h + cpp
- [ ] Create src/settings.h + cpp
- [ ] Refactor main.cpp to use new modules
- [ ] Test all functionality still works

### Sprint 7: Features (Day 5 - 2 hours)
- [ ] Add watchdog timer
- [ ] Add self-test on boot
- [ ] Add time validation (NTP sync check)
- [ ] Add match completion fanfare (3 blasts)
- [ ] Add debug mode macros

### Sprint 8: UX (Day 5 - 2 hours)
- [ ] Add settings save confirmation
- [ ] Add button disabled states
- [ ] Add keyboard shortcuts (space, R, S)
- [ ] Add loading overlay
- [ ] Add sound effects (optional)

### Sprint 9: Performance (Day 6 - 1 hour)
- [ ] Implement message batching
- [ ] Replace String with const char*
- [ ] Cache static files in RAM (optional)
- [ ] Optimize JSON allocations

### Sprint 10: Documentation & Testing (Day 6-7 - 3 hours)
- [ ] Add Doxygen comments to all functions
- [ ] Create ARCHITECTURE.md
- [ ] Create API.md
- [ ] Write unit tests
- [ ] Run integration tests
- [ ] Update README
- [ ] Final verification

**Phase 2 Verification:**
- [ ] Code well-organized ✓
- [ ] All features working ✓
- [ ] Documentation complete ✓
- [ ] Performance improved ✓

---

## Daily Goals

**Day 1 (3.5 hours):**
- Morning: Security + WiFi Portal
- Afternoon: Test portal with multiple devices

**Day 2 (3.5 hours):**
- Morning: Timer sync implementation
- Afternoon: Error handling + testing

**Day 3 (1 hour):**
- Final Phase 1 polish and testing
- Commit to master

**Day 4 (3 hours):**
- Code refactoring only
- No new features

**Day 5 (4 hours):**
- Morning: New features
- Afternoon: UX improvements

**Day 6-7 (4 hours):**
- Performance + documentation
- Final testing
- Merge to master

---

## Quick Test Commands

```bash
# Build and upload firmware
pio run --target upload

# Upload filesystem (if web files changed)
pio run --target uploadfs

# Monitor serial output
pio device monitor

# Build without uploading
pio run

# Clean build
pio run --target clean
```

---

## Quick Git Workflow

```bash
# Phase 1 (on master branch)
git add .
git commit -m "feat: implement security hardening"
git push -u origin claude/code-review-fixes-011CUbvmsTasEVcFuunnQhQk

# Phase 2 (create feature branch)
git checkout -b feature/enhancements
git add .
git commit -m "refactor: split code into modules"
git push -u origin feature/enhancements

# After Phase 2 complete, merge to master
git checkout claude/code-review-fixes-011CUbvmsTasEVcFuunnQhQk
git merge feature/enhancements
git push
```

---

## Emergency Rollback

```bash
# If Phase 1 breaks something
git revert HEAD
git push

# If Phase 2 has issues
git checkout claude/code-review-fixes-011CUbvmsTasEVcFuunnQhQk
git branch -D feature/enhancements
```

---

## Current Status

**Phase**: Not Started
**Last Updated**: [Date]
**Current Task**: Security hardening
**Blockers**: None

**Completed Sprints**: 0 / 10
**Phase 1 Progress**: 0%
**Phase 2 Progress**: 0%

---

## Quick Notes

Use this space for quick notes during implementation:

-
-
-

