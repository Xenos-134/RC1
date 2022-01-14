# Report
Date: Mon 10 Jan 2022 04:47:17 AM WET  
Repo: git@git.rnl.tecnico.ulisboa.pt:RC-21-22/ist196842-proj1.git  
Commit: 2c253c12292456231c3ced7acd8b4a409d9cceff  

## Build
* Found `Makefile`.
* Build succeeded.
* Found `file-sender`.
* Found `file-receiver`.

## Tests
| Test | Result |
| ---- |:------:|
| Sending small text file | OK |
| Sending binary file | OK |
| Sending 500 byte file | OK |
| Sending 1000 byte file | OK |
| Stop & Wait. No Loss | **FAIL** |
| Stop & Wait. Loss | **FAIL** |
| Go Back N. No Loss | **FAIL** |
| Go Back N. Loss | **FAIL** |
| Selective Repeat. No Loss | **FAIL** |
| Selective Repeat. Loss | **FAIL** |
| Message format | **FAIL** |

## Submission
* **`project1-submission` tag missing. Project not yet submitted.**
