# Logging Interop Contract (Draft)

## Operations

### log_info
Input:
```
{ "message": string }
```
Output:
```
{ "status": InteropStatusCode }
```
Errors: NOT_INITIALIZED, SHUTDOWN

### log_warn
Input same shape as log_info

### log_error
Input same shape as log_info

## Rules
- Messages tagged with script id + severity at native boundary
- Initial implementation: no rate limiting
- Future: optional structured fields (category, event_id)
