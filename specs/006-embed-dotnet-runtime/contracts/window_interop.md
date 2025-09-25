# Window Interop Contract (Draft)

## Operations

### create_window
Input:
```
{
  "width": int,
  "height": int,
  "title": string
}
```
Output:
```
{
  "status": InteropStatusCode,
  "window_id": int (present if status==OK)
}
```
Errors: NOT_INITIALIZED, INTERNAL_ERROR

### set_window_title
Input:
```
{
  "window_id": int,
  "title": string
}
```
Output:
```
{ "status": InteropStatusCode }
```
Errors: INVALID_ID, NOT_INITIALIZED, INTERNAL_ERROR

### close_window
Input:
```
{ "window_id": int }
```
Output:
```
{ "status": InteropStatusCode }
```
Errors: INVALID_ID, NOT_INITIALIZED, INTERNAL_ERROR

### window_exists
Input:
```
{ "window_id": int }
```
Output:
```
{ "status": InteropStatusCode, "exists": bool }
```
Errors: NOT_INITIALIZED

## Notes
- All titles UTF-8.
- Future: metadata readback (dimensions) not in MVP.
