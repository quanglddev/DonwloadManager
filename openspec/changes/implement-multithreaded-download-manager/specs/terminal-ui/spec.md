# Spec Delta: Terminal UI Capability

## ADDED Requirements

### Requirement: The system SHALL provide Real-Time Download Progress Display

The system SHALL display download progress in a terminal-based UI using FTXUI.

#### Scenario: Show active downloads with progress bars
**Given** 3 active downloads at various completion levels
**When** the UI renders
**Then** each download is displayed with a progress bar
**And** progress bars accurately reflect completion percentage
**And** download speeds are shown in human-readable units (KB/s, MB/s)
**And** ETA is calculated and displayed

#### Scenario: Update progress at 10fps
**Given** downloads transferring data rapidly
**When** the UI is rendering
**Then** progress updates occur at least 10 times per second
**And** UI updates do not cause flicker or tearing
**And** CPU usage for UI rendering is < 5%

#### Scenario: Show download states
**Given** downloads in various states (downloading, paused, completed, failed)
**When** the UI renders
**Then** each state is visually distinct (colors, symbols, text)
**And** paused downloads show "Paused" instead of speed
**And** completed downloads show checkmark and final size
**And** failed downloads show error message

---

### Requirement: The system SHALL provide Interactive Download Management

The system SHALL allow managing downloads via keyboard input.

#### Scenario: Add new download
**Given** the UI is running
**When** the user presses 'A'
**Then** a URL input prompt appears
**And** the user can enter a URL and destination path
**And** the download is added to the queue upon confirmation
**And** the UI returns to the main view

#### Scenario: Pause selected download
**Given** a download is selected
**When** the user presses 'P'
**Then** the selected download pauses within 2 seconds
**And** the state changes to "Paused"
**And** the UI reflects the new state immediately

#### Scenario: Resume selected download
**Given** a paused download is selected
**When** the user presses 'R'
**Then** the download resumes
**And** progress continues from the paused point
**And** the UI shows "Downloading" state

#### Scenario: Cancel selected download
**Given** a download is selected
**When** the user presses 'C'
**Then** a confirmation prompt appears
**When** confirmed
**Then** the download is canceled and removed
**And** partial files are deleted

#### Scenario: Navigate download list
**Given** multiple downloads in the list
**When** the user presses Up/Down arrows
**Then** the selection moves to the previous/next download
**And** the selected download is highlighted
**And** navigation wraps around at list boundaries

---

### Requirement: The system SHALL provide Global Statistics Display

The system SHALL display aggregate statistics across all downloads.

#### Scenario: Show total progress
**Given** 5 downloads with varying progress
**When** the UI renders
**Then** total bytes downloaded and total bytes expected are shown
**And** overall completion percentage is calculated
**And** statistics update in real-time

#### Scenario: Show aggregate transfer rate
**Given** multiple active downloads
**When** the UI renders
**Then** the combined transfer rate is displayed
**And** the rate reflects all active downloads
**And** the rate is accurate to within 5%

#### Scenario: Show ETA for all downloads
**Given** active downloads with known sizes
**When** the UI calculates ETA
**Then** the time remaining for all downloads to complete is shown
**And** ETA adapts to changing transfer rates
**And** ETA displays "Unknown" if sizes are unknown

---

### Requirement: The system SHALL provide Settings Management UI

The system SHALL provide an interface for configuring settings.

#### Scenario: Open settings panel
**Given** the UI is running
**When** the user presses 'S'
**Then** a settings panel overlays the main view
**And** current settings are displayed with values

#### Scenario: Modify max concurrent downloads
**Given** the settings panel is open
**When** the user selects "Max Concurrent Downloads"
**And** enters a new value (e.g., 5)
**Then** the setting is updated immediately
**And** the download manager adjusts active downloads
**And** the change is persisted to the state file

#### Scenario: Modify global bandwidth limit
**Given** the settings panel is open
**When** the user sets global bandwidth to 2MB/s
**Then** the limit is applied within 3 seconds
**And** all active downloads adjust their rates
**And** the UI shows the new rate in effect

#### Scenario: Toggle email notifications
**Given** the settings panel is open
**When** the user toggles email notifications on/off
**Then** the setting is saved
**And** future download completions respect the setting

---

### Requirement: The system SHALL provide Responsive Layout

The system SHALL adapt the UI layout to terminal size.

#### Scenario: Minimum terminal size
**Given** the terminal is resized to 80x24
**When** the UI renders
**Then** all essential information is visible
**And** long filenames are truncated with ellipsis
**And** no layout overflow occurs

#### Scenario: Large terminal size
**Given** the terminal is 200x60
**When** the UI renders
**Then** additional details are shown (full URLs, timestamps)
**And** progress bars expand to fill available width
**And** whitespace is used effectively

#### Scenario: Dynamic resize
**Given** the UI is rendering
**When** the terminal is resized
**Then** the layout adapts within 100ms
**And** no crashes or graphical glitches occur

---

### Requirement: The system SHALL provide Error and Status Messages

The system SHALL display informative messages for errors and events.

#### Scenario: Show network error
**Given** a download fails due to network error
**When** the error occurs
**Then** an error message appears at the bottom of the screen
**And** the message includes the filename and error reason
**And** the message auto-dismisses after 10 seconds

#### Scenario: Show success notification
**Given** a download completes successfully
**When** the completion is detected
**Then** a success message is displayed
**And** the message includes filename and final size
**And** the message is dismissible by pressing any key

#### Scenario: Show disk space warning
**Given** available disk space drops below 1GB
**When** the UI detects low space
**Then** a persistent warning appears
**And** downloads are paused until space is freed

---

### Requirement: The system SHALL provide Help and Keybindings

The system SHALL provide accessible help for UI controls.

#### Scenario: Display help overlay
**Given** the UI is running
**When** the user presses '?' or F1
**Then** a help overlay appears
**And** all keybindings are listed with descriptions
**And** the overlay is dismissible by pressing Esc or '?'

#### Scenario: Show contextual hints
**Given** the main UI view
**When** the UI renders
**Then** a status bar shows common keybindings
**And** hints are context-sensitive (e.g., "P:Pause" only for active downloads)

---

### Requirement: The system SHALL provide Color and Styling

The system SHALL use colors to enhance information hierarchy.

#### Scenario: Color-coded download states
**Given** downloads in different states
**When** the UI renders
**Then** downloading uses blue/cyan
**And** completed uses green
**And** failed uses red
**And** paused uses yellow
**And** queued uses gray

#### Scenario: Accessible in monochrome
**Given** a terminal without color support
**When** the UI renders
**Then** information is distinguishable by symbols and text
**And** no information is conveyed by color alone

---

### Requirement: The system SHALL provide Performance Under Load

The system SHALL remain responsive with many downloads.

#### Scenario: Display 100 downloads
**Given** 100 downloads in the list
**When** the UI renders
**Then** rendering completes within 50ms per frame
**And** scrolling is smooth (no lag)
**And** memory usage is reasonable (< 10MB for UI)

#### Scenario: High-frequency updates
**Given** 10 downloads updating progress every 100ms
**When** the UI is rendering
**Then** UI thread does not block download threads
**And** progress reads use lock-free atomic operations
**And** UI remains responsive to input

---

## UI Layout Mockup

```
┌─────────────────────────────────────────────────────────────┐
│ Download Manager v1.0               Active: 3/4    ↑/↓ ⏎    │
├─────────────────────────────────────────────────────────────┤
│ file1.zip                [▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░] 52% 2.1MB/s │
│ file2.tar.gz             [▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓] 100% ✓       │
│ file3.iso                [▓▓▓░░░░░░░░░░░░░░░░░] 15% 850KB/s │
│ file4.pdf                [Paused]                   0.0MB/s │
├─────────────────────────────────────────────────────────────┤
│ [A]dd  [P]ause  [R]esume  [C]ancel  [S]ettings  [Q]uit     │
├─────────────────────────────────────────────────────────────┤
│ Total: 1.2GB / 4.8GB     Rate: 3.5MB/s     ETA: 15m 24s    │
└─────────────────────────────────────────────────────────────┘
```

---

## Performance Targets

- **Render latency**: < 50ms per frame (20fps+)
- **Input latency**: < 100ms from keypress to action
- **CPU usage**: < 5% on modern hardware
- **Memory footprint**: < 10MB for UI components

---

## Cross-References

- **Depends on**: `core-download` (display download state)
- **Depends on**: `concurrency` (thread-safe state access)
- **Depends on**: `bandwidth-control` (display current speeds)
- **Depends on**: `state-management` (display loaded downloads)
- **Related to**: `notifications` (show completion events)
