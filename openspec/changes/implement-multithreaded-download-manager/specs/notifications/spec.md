# Spec Delta: Notifications Capability

## ADDED Requirements

### Requirement: The system SHALL provide Email Notifications

The system SHALL send email notifications for download events using the mailio library.

#### Scenario: Send email on download completion
**Given** a download completes successfully
**And** email notifications are enabled
**When** the download is finalized
**Then** an email is sent to the configured address
**And** the email subject includes the filename
**And** the email body includes download metadata (size, duration, speed)
**And** email sending does not block the download thread

#### Scenario: Send email on download failure
**Given** a download fails after exhausting retries
**And** email notifications are enabled
**When** the download is marked as failed
**Then** an email is sent to the configured address
**And** the email subject indicates failure
**And** the email body includes the error message and URL

#### Scenario: Batch multiple completions
**Given** 5 downloads complete within 1 minute
**When** email notifications are enabled
**Then** a single digest email is sent
**And** the email lists all completed downloads
**And** excessive email volume is avoided

---

### Requirement: The system SHALL provide SMTP Configuration

The system SHALL support configurable SMTP settings for email delivery.

#### Scenario: Configure SMTP server
**Given** the user wants to enable email notifications
**When** SMTP settings are configured
**Then** the user can specify server host, port, username, and password
**And** settings support common providers (Gmail, Outlook, custom SMTP)
**And** settings are securely persisted (password encrypted or keychain-stored)

#### Scenario: TLS/SSL support
**Given** an SMTP server requiring TLS
**When** email is sent
**Then** the connection uses TLS encryption
**And** certificate validation is performed
**And** connection fails gracefully if TLS is unavailable

#### Scenario: Test SMTP configuration
**Given** SMTP settings are entered
**When** the user triggers a configuration test
**Then** a test email is sent immediately
**And** success or failure is reported within 10 seconds
**And** error messages include troubleshooting hints (authentication, network)

---

### Requirement: The system SHALL provide Notification Queue

The system SHALL queue notifications for asynchronous delivery.

#### Scenario: Enqueue notification
**Given** a download completes
**When** an email notification is triggered
**Then** the notification is added to a queue
**And** the download thread continues immediately
**And** queue size is bounded (max 100 pending notifications)

#### Scenario: Process notification queue
**Given** notifications in the queue
**When** the notification worker processes the queue
**Then** emails are sent one at a time with rate limiting (1 email per 5 seconds)
**And** failed sends are retried up to 3 times with exponential backoff
**And** persistent failures are logged and discarded after 24 hours

#### Scenario: Graceful shutdown with pending notifications
**Given** the application is shutting down
**And** 3 notifications are pending
**When** shutdown is initiated
**Then** pending notifications are persisted to disk
**And** on next startup, queued notifications are loaded and sent

---

### Requirement: The system SHALL provide Notification Content

The system SHALL generate informative and well-formatted email content.

#### Scenario: Completion email format
**Given** a download completes
**When** the email is generated
**Then** the subject is "Download Complete: <filename>"
**And** the body includes:
  - Filename and destination path
  - File size and download duration
  - Average download speed
  - Timestamp of completion
**And** the email is plain text with optional HTML formatting

#### Scenario: Failure email format
**Given** a download fails
**When** the email is generated
**Then** the subject is "Download Failed: <filename>"
**And** the body includes:
  - Filename and URL
  - Error message and error code
  - Number of retry attempts
  - Timestamp of failure
  - Suggested troubleshooting steps

#### Scenario: Digest email format
**Given** multiple downloads complete within the batch window
**When** the digest email is generated
**Then** the subject is "Download Manager: 5 Downloads Complete"
**And** the body lists each download with:
  - Filename
  - Size
  - Completion timestamp
**And** total statistics (cumulative size, average speed)

---

### Requirement: The system SHALL provide Notification Preferences

The system SHALL allow fine-grained control over notification behavior.

#### Scenario: Enable/disable notifications globally
**Given** email notifications are configured
**When** the user disables notifications
**Then** no emails are sent regardless of events
**And** the setting persists across restarts

#### Scenario: Selective notification types
**Given** the user wants only failure notifications
**When** notification preferences are set to "failures only"
**Then** completion emails are not sent
**And** failure emails are sent normally

#### Scenario: Quiet hours
**Given** quiet hours are configured (e.g., 10 PM - 8 AM)
**When** a download completes during quiet hours
**Then** the notification is queued but not sent
**And** queued notifications are sent at the end of quiet hours

---

### Requirement: The system SHALL provide Error Handling

The system SHALL handle email delivery failures gracefully.

#### Scenario: SMTP connection failure
**Given** the SMTP server is unreachable
**When** an email send is attempted
**Then** the send fails after a 30-second timeout
**And** the failure is logged
**And** the notification is retried 3 times with 5-minute delays
**And** the user is not blocked from using the application

#### Scenario: Authentication failure
**Given** SMTP credentials are incorrect
**When** an email send is attempted
**Then** authentication fails immediately
**And** the error is logged with details (invalid username/password)
**And** notifications are paused until credentials are corrected
**And** the user is alerted in the UI

#### Scenario: Rate limiting by email provider
**Given** the email provider enforces rate limits (e.g., 10 emails/minute)
**When** the rate limit is exceeded
**Then** sends are delayed to comply with limits
**And** the queue processes slowly but reliably
**And** no emails are lost

---

### Requirement: The system SHALL provide Privacy and Security

The system SHALL handle email credentials and content securely.

#### Scenario: Secure credential storage
**Given** SMTP credentials are entered
**When** credentials are saved
**Then** passwords are encrypted at rest (AES-256 or platform keychain)
**And** credentials are never logged or displayed in plaintext
**And** credentials are transmitted over TLS only

#### Scenario: Sanitize email content
**Given** a download URL contains sensitive tokens (e.g., API keys in query params)
**When** an email is generated
**Then** sensitive parts of the URL are redacted or removed
**And** full URLs are included only if explicitly enabled by user

---

## Email Templates

### Completion Template (Plain Text)
```
Subject: Download Complete: file.zip

Hello,

Your download has completed successfully:

File:     file.zip
Size:     150.3 MB
Duration: 5m 42s
Speed:    450 KB/s
Saved to: /home/user/Downloads/file.zip
Completed: 2025-10-23 14:30:15 UTC

--
Download Manager v1.0
```

### Failure Template (Plain Text)
```
Subject: Download Failed: file.zip

Hello,

Your download has failed:

File: file.zip
URL:  https://example.com/file.zip
Error: Connection timeout (HTTP 0)
Retry attempts: 3
Failed at: 2025-10-23 14:30:15 UTC

Troubleshooting:
- Check your internet connection
- Verify the URL is still valid
- Try resuming the download manually

--
Download Manager v1.0
```

---

## Performance Targets

- **Email send latency**: < 5 seconds under normal conditions
- **Queue processing**: 1 email per 5 seconds (rate limiting)
- **Memory usage**: < 1MB for notification queue
- **Retry delay**: 5 minutes between retries

---

## Cross-References

- **Depends on**: `core-download` (completion/failure events)
- **Depends on**: `state-management` (persist SMTP settings and notification queue)
- **Related to**: `terminal-ui` (display notification status)
- **Related to**: `concurrency` (async notification delivery)
