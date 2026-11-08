# VirusTotal 逐文件扫描报告 / Individual file scan report

- 范围：上传的 `win.7z` 与 `mac.zip` 内所有文件；相同 SHA-256 的内容合并展示。
- Scope: all files in the uploaded `win.7z` and `mac.zip`, grouped by identical SHA-256.
- 覆盖：92 个压缩包内路径，对应 47 份不同内容；其中 17 份使用先前的完成报告。
- Coverage: 92 archive paths representing 47 unique contents; 17 use previously completed reports.
- “未检出”不等于证明文件安全；各条目保留超时、失败和不支持的数量。
- “Undetected” does not prove a file is safe; timeout, failure and unsupported counts are retained.
- 这些是查询时的报告快照，VirusTotal 网页结果可能发生变化。
- These are report snapshots at the time of retrieval; VirusTotal results may change.

## 检测出问题 / Detections

### `00_ToLissOANS-debug/CollectLogs.exe`

- SHA-256: `4761aa165c3d5e379d94fffdd50db8107da92e0a940bd3af6a90603268b5d2a3`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/4761aa165c3d5e379d94fffdd50db8107da92e0a940bd3af6a90603268b5d2a3/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/4761aa165c3d5e379d94fffdd50db8107da92e0a940bd3af6a90603268b5d2a3/detection)
- 报告时间: 2026-11-07 15:37 UTC；已完成的报告
- Report date: 2026-11-07 15:37 UTC; completed report
- 结果: 恶意=11, 可疑=0, 未检出=59, 超时=0, 确认超时=0, 失败=1, 不支持=4
- Results: malicious=11, suspicious=0, undetected=59, timeout=0, confirmed-timeout=0, failure=1, type-unsupported=4
- 指定引擎: BitDefender=恶意 (Gen:Variant.Zusy.599002), Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=malicious (Gen:Variant.Zusy.599002), Microsoft=undetected, Malwarebytes=undetected
- 检出名称: Bkav: W32.Malware.511F7717, ALYac: Gen:Variant.Zusy.599002, BitDefender: Gen:Variant.Zusy.599002, Arcabit: Trojan.Zusy.D923DA, MicroWorld-eScan: Gen:Variant.Zusy.599002, VIPRE: Gen:Variant.Zusy.599002, McAfeeD: ti!4761AA165C3D, CTX: exe.unknown.generic, Emsisoft: Gen:Variant.Zusy.599002 (B), Antiy-AVL: Trojan/Win32.Agent, GData: Gen:Variant.Zusy.599002
- Detections: Bkav: W32.Malware.511F7717, ALYac: Gen:Variant.Zusy.599002, BitDefender: Gen:Variant.Zusy.599002, Arcabit: Trojan.Zusy.D923DA, MicroWorld-eScan: Gen:Variant.Zusy.599002, VIPRE: Gen:Variant.Zusy.599002, McAfeeD: ti!4761AA165C3D, CTX: exe.unknown.generic, Emsisoft: Gen:Variant.Zusy.599002 (B), Antiy-AVL: Trojan/Win32.Agent, GData: Gen:Variant.Zusy.599002
- 压缩包内路径: win: `00_ToLissOANS-debug/CollectLogs.exe`
- Archive paths: win: `00_ToLissOANS-debug/CollectLogs.exe`

### `00_ToLissOANS/64/mac.xpl`

- SHA-256: `6bebf8497f6388889b69e3df430217841fab5e66b15ec3b4c891cc0009a802ab`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/6bebf8497f6388889b69e3df430217841fab5e66b15ec3b4c891cc0009a802ab/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/6bebf8497f6388889b69e3df430217841fab5e66b15ec3b4c891cc0009a802ab/detection)
- 报告时间: 2026-11-07 15:23 UTC；已完成的报告
- Report date: 2026-11-07 15:23 UTC; completed report
- 结果: 恶意=1, 可疑=0, 未检出=62, 超时=0, 确认超时=0, 失败=0, 不支持=12
- Results: malicious=1, suspicious=0, undetected=62, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=12
- 指定引擎: BitDefender=未检出, Microsoft=恶意 (Trojan:Script/Wacatac.C!ml), Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=malicious (Trojan:Script/Wacatac.C!ml), Malwarebytes=undetected
- 检出名称: Microsoft: Trojan:Script/Wacatac.C!ml
- Detections: Microsoft: Trojan:Script/Wacatac.C!ml
- 压缩包内路径: mac: `00_ToLissOANS/64/mac.xpl`
- Archive paths: mac: `00_ToLissOANS/64/mac.xpl`

## 未检测出问题 / No detections

### `00_ToLissOANS-debug/64/win.xpl`

- SHA-256: `7ba5f49d0034df24f8f1768c08979e0c5a8f7e32e1c4346f661a406fcd5d46ce`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/7ba5f49d0034df24f8f1768c08979e0c5a8f7e32e1c4346f661a406fcd5d46ce/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/7ba5f49d0034df24f8f1768c08979e0c5a8f7e32e1c4346f661a406fcd5d46ce/detection)
- 报告时间: 2026-11-07 15:16 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-07 15:16 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=70, 超时=0, 确认超时=1, 失败=0, 不支持=4
- Results: malicious=0, suspicious=0, undetected=70, timeout=0, confirmed-timeout=1, failure=0, type-unsupported=4
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS-debug/64/win.xpl`
- Archive paths: win: `00_ToLissOANS-debug/64/win.xpl`

### `00_ToLissOANS-debug/config.ini`

- SHA-256: `42eeb2cf72330ce0937b051277ff09fff63d18d810f2dfe4b5612f4ab1f94fee`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/42eeb2cf72330ce0937b051277ff09fff63d18d810f2dfe4b5612f4ab1f94fee/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/42eeb2cf72330ce0937b051277ff09fff63d18d810f2dfe4b5612f4ab1f94fee/detection)
- 报告时间: 2026-11-07 15:21 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-07 15:21 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS-debug/config.ini`; win: `00_ToLissOANS/config.ini`; mac: `00_ToLissOANS/config.ini`
- Archive paths: win: `00_ToLissOANS-debug/config.ini`; win: `00_ToLissOANS/config.ini`; mac: `00_ToLissOANS/config.ini`

### `00_ToLissOANS-debug/INSTALL.txt`

- SHA-256: `abb393a403b4e03db72ac4fcfbc4509a07f6f47e717d669ae02b57c8f69892c6`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/abb393a403b4e03db72ac4fcfbc4509a07f6f47e717d669ae02b57c8f69892c6/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/abb393a403b4e03db72ac4fcfbc4509a07f6f47e717d669ae02b57c8f69892c6/detection)
- 报告时间: 2026-11-07 15:21 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-07 15:21 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS-debug/INSTALL.txt`
- Archive paths: win: `00_ToLissOANS-debug/INSTALL.txt`

### `00_ToLissOANS-debug/LICENSE`

- SHA-256: `230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=61, 超时=0, 确认超时=0, 失败=0, 不支持=14
- Results: malicious=0, suspicious=0, undetected=61, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS-debug/LICENSE`; win: `00_ToLissOANS/LICENSE`
- Archive paths: win: `00_ToLissOANS-debug/LICENSE`; win: `00_ToLissOANS/LICENSE`

### `00_ToLissOANS-debug/manifest.json`

- SHA-256: `172134a36ac179c84906b0ee60e30885dda82aff5e659c99529c580b276d9796`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/172134a36ac179c84906b0ee60e30885dda82aff5e659c99529c580b276d9796/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/172134a36ac179c84906b0ee60e30885dda82aff5e659c99529c580b276d9796/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=1, 不支持=14
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=1, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS-debug/manifest.json`
- Archive paths: win: `00_ToLissOANS-debug/manifest.json`

### `00_ToLissOANS-debug/THIRD_PARTY_NOTICES.txt`

- SHA-256: `5969722fcd4a27992d747895073a517e9f9e218ad130edeac7135c804d314ef2`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/5969722fcd4a27992d747895073a517e9f9e218ad130edeac7135c804d314ef2/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/5969722fcd4a27992d747895073a517e9f9e218ad130edeac7135c804d314ef2/detection)
- 报告时间: 2026-11-05 13:33 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-05 13:33 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS-debug/THIRD_PARTY_NOTICES.txt`; win: `00_ToLissOANS/THIRD_PARTY_NOTICES.txt`; mac: `00_ToLissOANS/THIRD_PARTY_NOTICES.txt`
- Archive paths: win: `00_ToLissOANS-debug/THIRD_PARTY_NOTICES.txt`; win: `00_ToLissOANS/THIRD_PARTY_NOTICES.txt`; mac: `00_ToLissOANS/THIRD_PARTY_NOTICES.txt`

### `00_ToLissOANS-debug/tools/7zip/7z.dll`

- SHA-256: `882063948d675ee41b5ae68db3e84879350ec81cf88d15b9babf2fa08e332863`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/882063948d675ee41b5ae68db3e84879350ec81cf88d15b9babf2fa08e332863/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/882063948d675ee41b5ae68db3e84879350ec81cf88d15b9babf2fa08e332863/detection)
- 报告时间: 2026-08-07 14:13 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-08-07 14:13 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=70, 超时=0, 确认超时=0, 失败=1, 不支持=4
- Results: malicious=0, suspicious=0, undetected=70, timeout=0, confirmed-timeout=0, failure=1, type-unsupported=4
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS-debug/tools/7zip/7z.dll`
- Archive paths: win: `00_ToLissOANS-debug/tools/7zip/7z.dll`

### `00_ToLissOANS-debug/tools/7zip/7z.exe`

- SHA-256: `e2ca3ec168ae9c0b4115cd4fe220145ea9b2dc4b6fc79d765e91f415b34d00de`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/e2ca3ec168ae9c0b4115cd4fe220145ea9b2dc4b6fc79d765e91f415b34d00de/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/e2ca3ec168ae9c0b4115cd4fe220145ea9b2dc4b6fc79d765e91f415b34d00de/detection)
- 报告时间: 2026-11-06 14:03 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-06 14:03 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=71, 超时=0, 确认超时=0, 失败=0, 不支持=4
- Results: malicious=0, suspicious=0, undetected=71, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=4
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS-debug/tools/7zip/7z.exe`
- Archive paths: win: `00_ToLissOANS-debug/tools/7zip/7z.exe`

### `00_ToLissOANS-debug/tools/7zip/License.txt`

- SHA-256: `3a9a7bca133a8af4560f48dfa351f941e110d80a2c2466e537ec6680b9fc2dda`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/3a9a7bca133a8af4560f48dfa351f941e110d80a2c2466e537ec6680b9fc2dda/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/3a9a7bca133a8af4560f48dfa351f941e110d80a2c2466e537ec6680b9fc2dda/detection)
- 报告时间: 2026-08-07 14:15 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-08-07 14:15 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=61, 超时=0, 确认超时=0, 失败=0, 不支持=14
- Results: malicious=0, suspicious=0, undetected=61, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS-debug/tools/7zip/License.txt`
- Archive paths: win: `00_ToLissOANS-debug/tools/7zip/License.txt`

### `00_ToLissOANS-debug/tools/7zip/SOURCE.txt`

- SHA-256: `4fdae498c044649c6cfa3e0f84284e2ac0d8be4adf915730ef114e26504d0303`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/4fdae498c044649c6cfa3e0f84284e2ac0d8be4adf915730ef114e26504d0303/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/4fdae498c044649c6cfa3e0f84284e2ac0d8be4adf915730ef114e26504d0303/detection)
- 报告时间: 2026-11-05 13:33 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-05 13:33 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS-debug/tools/7zip/SOURCE.txt`
- Archive paths: win: `00_ToLissOANS-debug/tools/7zip/SOURCE.txt`

### `00_ToLissOANS/64/win.xpl`

- SHA-256: `face41fcc5c637306d4b56074f6245cfd940e7b91d99878640b1b1efb64d12c3`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/face41fcc5c637306d4b56074f6245cfd940e7b91d99878640b1b1efb64d12c3/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/face41fcc5c637306d4b56074f6245cfd940e7b91d99878640b1b1efb64d12c3/detection)
- 报告时间: 2026-11-07 15:15 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-07 15:15 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=71, 超时=0, 确认超时=0, 失败=0, 不支持=4
- Results: malicious=0, suspicious=0, undetected=71, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=4
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/64/win.xpl`
- Archive paths: win: `00_ToLissOANS/64/win.xpl`

### `00_ToLissOANS/assets/a350/DSFLIB_LICENSE.txt`

- SHA-256: `77b0f474c9e42f51c75f508fa0ee6e05fd3dc85e0497a26caa92cef01e2b778e`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/77b0f474c9e42f51c75f508fa0ee6e05fd3dc85e0497a26caa92cef01e2b778e/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/77b0f474c9e42f51c75f508fa0ee6e05fd3dc85e0497a26caa92cef01e2b778e/detection)
- 报告时间: 2026-11-05 13:33 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-05 13:33 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=61, 超时=0, 确认超时=0, 失败=0, 不支持=14
- Results: malicious=0, suspicious=0, undetected=61, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/assets/a350/DSFLIB_LICENSE.txt`; win: `00_ToLissOANS-debug/assets/a350/DSFLIB_LICENSE.txt`; mac: `00_ToLissOANS/assets/a350/DSFLIB_LICENSE.txt`
- Archive paths: win: `00_ToLissOANS/assets/a350/DSFLIB_LICENSE.txt`; win: `00_ToLissOANS-debug/assets/a350/DSFLIB_LICENSE.txt`; mac: `00_ToLissOANS/assets/a350/DSFLIB_LICENSE.txt`

### `00_ToLissOANS/assets/a350/LICENSE.txt`

- SHA-256: `14864e24dd536e82e0056cb4049caeeca672c1a1710e78c511e7ec45825c295d`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/14864e24dd536e82e0056cb4049caeeca672c1a1710e78c511e7ec45825c295d/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/14864e24dd536e82e0056cb4049caeeca672c1a1710e78c511e7ec45825c295d/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/assets/a350/LICENSE.txt`
- Archive paths: mac: `00_ToLissOANS/assets/a350/LICENSE.txt`

### `00_ToLissOANS/assets/a350/LICENSE.txt`

- SHA-256: `4ffbaf7c64ce47fc0b155bdcc2c2bd9b8b38086e95b648e663bea1b51fd9848e`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/4ffbaf7c64ce47fc0b155bdcc2c2bd9b8b38086e95b648e663bea1b51fd9848e/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/4ffbaf7c64ce47fc0b155bdcc2c2bd9b8b38086e95b648e663bea1b51fd9848e/detection)
- 报告时间: 2026-11-05 13:33 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-05 13:33 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/assets/a350/LICENSE.txt`; win: `00_ToLissOANS-debug/assets/a350/LICENSE.txt`
- Archive paths: win: `00_ToLissOANS/assets/a350/LICENSE.txt`; win: `00_ToLissOANS-debug/assets/a350/LICENSE.txt`

### `00_ToLissOANS/assets/a350/LUA_LICENSE.txt`

- SHA-256: `8675d8f52f1607f45dcda2e13e59ac38e34c9252023a9c6d88331cac05546cff`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/8675d8f52f1607f45dcda2e13e59ac38e34c9252023a9c6d88331cac05546cff/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/8675d8f52f1607f45dcda2e13e59ac38e34c9252023a9c6d88331cac05546cff/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/assets/a350/LUA_LICENSE.txt`
- Archive paths: mac: `00_ToLissOANS/assets/a350/LUA_LICENSE.txt`

### `00_ToLissOANS/assets/a350/LUA_LICENSE.txt`

- SHA-256: `d1b7eafee13b329963abf7bcb21a5d4e61556b54bc31810a53519f3bf850c2b0`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/d1b7eafee13b329963abf7bcb21a5d4e61556b54bc31810a53519f3bf850c2b0/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/d1b7eafee13b329963abf7bcb21a5d4e61556b54bc31810a53519f3bf850c2b0/detection)
- 报告时间: 2026-11-05 13:33 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-05 13:33 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/assets/a350/LUA_LICENSE.txt`; win: `00_ToLissOANS-debug/assets/a350/LUA_LICENSE.txt`
- Archive paths: win: `00_ToLissOANS/assets/a350/LUA_LICENSE.txt`; win: `00_ToLissOANS-debug/assets/a350/LUA_LICENSE.txt`

### `00_ToLissOANS/assets/a350/LUA_README.html`

- SHA-256: `760d00c8b494ec2f170bcdd969e36b038027b0288232c60e15ffa6b0d784e9c7`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/760d00c8b494ec2f170bcdd969e36b038027b0288232c60e15ffa6b0d784e9c7/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/760d00c8b494ec2f170bcdd969e36b038027b0288232c60e15ffa6b0d784e9c7/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=61, 超时=0, 确认超时=0, 失败=0, 不支持=14
- Results: malicious=0, suspicious=0, undetected=61, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/assets/a350/LUA_README.html`; win: `00_ToLissOANS-debug/assets/a350/LUA_README.html`; mac: `00_ToLissOANS/assets/a350/LUA_README.html`
- Archive paths: win: `00_ToLissOANS/assets/a350/LUA_README.html`; win: `00_ToLissOANS-debug/assets/a350/LUA_README.html`; mac: `00_ToLissOANS/assets/a350/LUA_README.html`

### `00_ToLissOANS/assets/a350/map_engine.lua`

- SHA-256: `963e11ffe7652f4ea4fdc9834717d9855ce473b3238c8c894fd637354621e5fd`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/963e11ffe7652f4ea4fdc9834717d9855ce473b3238c8c894fd637354621e5fd/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/963e11ffe7652f4ea4fdc9834717d9855ce473b3238c8c894fd637354621e5fd/detection)
- 报告时间: 2026-11-07 15:16 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-07 15:16 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=61, 超时=0, 确认超时=0, 失败=0, 不支持=14
- Results: malicious=0, suspicious=0, undetected=61, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/assets/a350/map_engine.lua`; win: `00_ToLissOANS-debug/assets/a350/map_engine.lua`; mac: `00_ToLissOANS/assets/a350/map_engine.lua`
- Archive paths: win: `00_ToLissOANS/assets/a350/map_engine.lua`; win: `00_ToLissOANS-debug/assets/a350/map_engine.lua`; mac: `00_ToLissOANS/assets/a350/map_engine.lua`

### `00_ToLissOANS/assets/a350/provenance.json`

- SHA-256: `f320760f5651a28dd9fb3eec5ba96e4f8f83084670cc9c6ef3a45c07e33559f4`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/f320760f5651a28dd9fb3eec5ba96e4f8f83084670cc9c6ef3a45c07e33559f4/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/f320760f5651a28dd9fb3eec5ba96e4f8f83084670cc9c6ef3a45c07e33559f4/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=61, 超时=0, 确认超时=0, 失败=0, 不支持=14
- Results: malicious=0, suspicious=0, undetected=61, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/assets/a350/provenance.json`; win: `00_ToLissOANS-debug/assets/a350/provenance.json`; mac: `00_ToLissOANS/assets/a350/provenance.json`
- Archive paths: win: `00_ToLissOANS/assets/a350/provenance.json`; win: `00_ToLissOANS-debug/assets/a350/provenance.json`; mac: `00_ToLissOANS/assets/a350/provenance.json`

### `00_ToLissOANS/assets/font_b612_OFL.txt`

- SHA-256: `41eb0cf56ee1bae213d4a5c650fe60d936bdeda605d3cc6abd7a1720f6083e62`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/41eb0cf56ee1bae213d4a5c650fe60d936bdeda605d3cc6abd7a1720f6083e62/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/41eb0cf56ee1bae213d4a5c650fe60d936bdeda605d3cc6abd7a1720f6083e62/detection)
- 报告时间: 2026-07-08 16:46 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-07-08 16:46 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=57, 超时=0, 确认超时=0, 失败=4, 不支持=12
- Results: malicious=0, suspicious=0, undetected=57, timeout=0, confirmed-timeout=0, failure=4, type-unsupported=12
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/assets/font_b612_OFL.txt`
- Archive paths: mac: `00_ToLissOANS/assets/font_b612_OFL.txt`

### `00_ToLissOANS/assets/font_b612_OFL.txt`

- SHA-256: `a815f65bc72d90494b01842d3171f7cb0f9f935e023d9cc260904d221ef1064a`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/a815f65bc72d90494b01842d3171f7cb0f9f935e023d9cc260904d221ef1064a/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/a815f65bc72d90494b01842d3171f7cb0f9f935e023d9cc260904d221ef1064a/detection)
- 报告时间: 2022-10-25 11:48 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2022-10-25 11:48 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=61, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=61, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/assets/font_b612_OFL.txt`; win: `00_ToLissOANS-debug/assets/font_b612_OFL.txt`
- Archive paths: win: `00_ToLissOANS/assets/font_b612_OFL.txt`; win: `00_ToLissOANS-debug/assets/font_b612_OFL.txt`

### `00_ToLissOANS/assets/font_b612_px2.bin`

- SHA-256: `6583bae930621a916d861d1aaf54cb8fd1a825a3bd71e18b4c3548c3264e9d43`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/6583bae930621a916d861d1aaf54cb8fd1a825a3bd71e18b4c3548c3264e9d43/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/6583bae930621a916d861d1aaf54cb8fd1a825a3bd71e18b4c3548c3264e9d43/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/assets/font_b612_px2.bin`; win: `00_ToLissOANS/assets/a350/A350_BTV_data/font_b612_px2.bin`; win: `00_ToLissOANS-debug/assets/font_b612_px2.bin`; win: `00_ToLissOANS-debug/assets/a350/A350_BTV_data/font_b612_px2.bin`; mac: `00_ToLissOANS/assets/font_b612_px2.bin`; mac: `00_ToLissOANS/assets/a350/A350_BTV_data/font_b612_px2.bin`
- Archive paths: win: `00_ToLissOANS/assets/font_b612_px2.bin`; win: `00_ToLissOANS/assets/a350/A350_BTV_data/font_b612_px2.bin`; win: `00_ToLissOANS-debug/assets/font_b612_px2.bin`; win: `00_ToLissOANS-debug/assets/a350/A350_BTV_data/font_b612_px2.bin`; mac: `00_ToLissOANS/assets/font_b612_px2.bin`; mac: `00_ToLissOANS/assets/a350/A350_BTV_data/font_b612_px2.bin`

### `00_ToLissOANS/assets/wtc-source.txt`

- SHA-256: `20d895751c964cc5cdfd2668adc674ab74d956ed206ef8903d85b012e46ff16f`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/20d895751c964cc5cdfd2668adc674ab74d956ed206ef8903d85b012e46ff16f/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/20d895751c964cc5cdfd2668adc674ab74d956ed206ef8903d85b012e46ff16f/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/assets/wtc-source.txt`
- Archive paths: mac: `00_ToLissOANS/assets/wtc-source.txt`

### `00_ToLissOANS/assets/wtc-source.txt`

- SHA-256: `30c9c700adf4590fb740132c88c0668bd9a24311f28707bb67127d7014cb5e97`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/30c9c700adf4590fb740132c88c0668bd9a24311f28707bb67127d7014cb5e97/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/30c9c700adf4590fb740132c88c0668bd9a24311f28707bb67127d7014cb5e97/detection)
- 报告时间: 2026-11-05 13:33 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-05 13:33 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/assets/wtc-source.txt`; win: `00_ToLissOANS-debug/assets/wtc-source.txt`
- Archive paths: win: `00_ToLissOANS/assets/wtc-source.txt`; win: `00_ToLissOANS-debug/assets/wtc-source.txt`

### `00_ToLissOANS/assets/wtc.json`

- SHA-256: `42afbf9b93cff08de8ef8fb0b8cad1670dd1d07785c40b44a82ef11ad31eec81`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/42afbf9b93cff08de8ef8fb0b8cad1670dd1d07785c40b44a82ef11ad31eec81/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/42afbf9b93cff08de8ef8fb0b8cad1670dd1d07785c40b44a82ef11ad31eec81/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/assets/wtc.json`; win: `00_ToLissOANS-debug/assets/wtc.json`; mac: `00_ToLissOANS/assets/wtc.json`
- Archive paths: win: `00_ToLissOANS/assets/wtc.json`; win: `00_ToLissOANS-debug/assets/wtc.json`; mac: `00_ToLissOANS/assets/wtc.json`

### `00_ToLissOANS/cache/dsf_buildings_ZUCK.json`

- SHA-256: `00661b070cf66375b66cb44fe385eec50bdc71ab9ff0b94ad84dba95c69d67e0`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/00661b070cf66375b66cb44fe385eec50bdc71ab9ff0b94ad84dba95c69d67e0/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/00661b070cf66375b66cb44fe385eec50bdc71ab9ff0b94ad84dba95c69d67e0/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/cache/dsf_buildings_ZUCK.json`
- Archive paths: mac: `00_ToLissOANS/cache/dsf_buildings_ZUCK.json`

### `00_ToLissOANS/cache/dsf_buildings_ZUCK.json`

- SHA-256: `70c98251cdd9ea6884d5af6a2d8259a66efceb3ae1ef8cc8b89d8f5816be42ad`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/70c98251cdd9ea6884d5af6a2d8259a66efceb3ae1ef8cc8b89d8f5816be42ad/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/70c98251cdd9ea6884d5af6a2d8259a66efceb3ae1ef8cc8b89d8f5816be42ad/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/cache/dsf_buildings_ZUCK.json`; win: `00_ToLissOANS-debug/cache/dsf_buildings_ZUCK.json`
- Archive paths: win: `00_ToLissOANS/cache/dsf_buildings_ZUCK.json`; win: `00_ToLissOANS-debug/cache/dsf_buildings_ZUCK.json`

### `00_ToLissOANS/cache/osm2_ZUCK.json`

- SHA-256: `05b1460aaacb60d864e9feaee049613354e26e28e0010c35528a6bcfbbafb227`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/05b1460aaacb60d864e9feaee049613354e26e28e0010c35528a6bcfbbafb227/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/05b1460aaacb60d864e9feaee049613354e26e28e0010c35528a6bcfbbafb227/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=58, 超时=2, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=58, timeout=2, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/cache/osm2_ZUCK.json`; win: `00_ToLissOANS-debug/cache/osm2_ZUCK.json`; mac: `00_ToLissOANS/cache/osm2_ZUCK.json`
- Archive paths: win: `00_ToLissOANS/cache/osm2_ZUCK.json`; win: `00_ToLissOANS-debug/cache/osm2_ZUCK.json`; mac: `00_ToLissOANS/cache/osm2_ZUCK.json`

### `00_ToLissOANS/config/lr/a333.json`

- SHA-256: `25a3212d48fc1904a1bdd6096bb37640ac88b0b9936fb15347c27fcad2ca61c7`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/25a3212d48fc1904a1bdd6096bb37640ac88b0b9936fb15347c27fcad2ca61c7/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/25a3212d48fc1904a1bdd6096bb37640ac88b0b9936fb15347c27fcad2ca61c7/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/config/lr/a333.json`; win: `00_ToLissOANS-debug/config/lr/a333.json`
- Archive paths: win: `00_ToLissOANS/config/lr/a333.json`; win: `00_ToLissOANS-debug/config/lr/a333.json`

### `00_ToLissOANS/config/lr/a333.json`

- SHA-256: `4f61708ce171c6bc28f93854e1a34d8359d1d622df49ed86792602581bf1fd89`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/4f61708ce171c6bc28f93854e1a34d8359d1d622df49ed86792602581bf1fd89/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/4f61708ce171c6bc28f93854e1a34d8359d1d622df49ed86792602581bf1fd89/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/config/lr/a333.json`
- Archive paths: mac: `00_ToLissOANS/config/lr/a333.json`

### `00_ToLissOANS/config/toliss/a20n.json`

- SHA-256: `17bc62f67dd4714cd9158f7d0699abf4b402b9a1171511405a6e2edab4e9b1f1`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/17bc62f67dd4714cd9158f7d0699abf4b402b9a1171511405a6e2edab4e9b1f1/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/17bc62f67dd4714cd9158f7d0699abf4b402b9a1171511405a6e2edab4e9b1f1/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=57, 超时=4, 确认超时=0, 失败=0, 不支持=14
- Results: malicious=0, suspicious=0, undetected=57, timeout=4, confirmed-timeout=0, failure=0, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/config/toliss/a20n.json`; mac: `00_ToLissOANS/config/toliss/a320.json`
- Archive paths: mac: `00_ToLissOANS/config/toliss/a20n.json`; mac: `00_ToLissOANS/config/toliss/a320.json`

### `00_ToLissOANS/config/toliss/a20n.json`

- SHA-256: `2c59791eeb9dd4b8ff74c36b64f3c6e40220c3394227d2f1cf20b2ca6accd682`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/2c59791eeb9dd4b8ff74c36b64f3c6e40220c3394227d2f1cf20b2ca6accd682/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/2c59791eeb9dd4b8ff74c36b64f3c6e40220c3394227d2f1cf20b2ca6accd682/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/config/toliss/a20n.json`; win: `00_ToLissOANS/config/toliss/a320.json`; win: `00_ToLissOANS-debug/config/toliss/a20n.json`; win: `00_ToLissOANS-debug/config/toliss/a320.json`
- Archive paths: win: `00_ToLissOANS/config/toliss/a20n.json`; win: `00_ToLissOANS/config/toliss/a320.json`; win: `00_ToLissOANS-debug/config/toliss/a20n.json`; win: `00_ToLissOANS-debug/config/toliss/a320.json`

### `00_ToLissOANS/config/toliss/a319.json`

- SHA-256: `de292f17073ab2a2ff4d5cb56e131e69fa1a735a5fc361f8d97922bcd01fc14e`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/de292f17073ab2a2ff4d5cb56e131e69fa1a735a5fc361f8d97922bcd01fc14e/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/de292f17073ab2a2ff4d5cb56e131e69fa1a735a5fc361f8d97922bcd01fc14e/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/config/toliss/a319.json`; win: `00_ToLissOANS-debug/config/toliss/a319.json`; mac: `00_ToLissOANS/config/toliss/a319.json`
- Archive paths: win: `00_ToLissOANS/config/toliss/a319.json`; win: `00_ToLissOANS-debug/config/toliss/a319.json`; mac: `00_ToLissOANS/config/toliss/a319.json`

### `00_ToLissOANS/config/toliss/a320-332a-ee23.json`

- SHA-256: `946f02d25bef68b2af26b53f726681e0308f16418c08794819b32787e1473ef6`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/946f02d25bef68b2af26b53f726681e0308f16418c08794819b32787e1473ef6/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/946f02d25bef68b2af26b53f726681e0308f16418c08794819b32787e1473ef6/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/config/toliss/a320-332a-ee23.json`; win: `00_ToLissOANS-debug/config/toliss/a320-332a-ee23.json`
- Archive paths: win: `00_ToLissOANS/config/toliss/a320-332a-ee23.json`; win: `00_ToLissOANS-debug/config/toliss/a320-332a-ee23.json`

### `00_ToLissOANS/config/toliss/a320-332a-ee23.json`

- SHA-256: `f30aa09be54e4a358d95e30772cd8140efc0440d1c39c09ebe7d24a84d298f87`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/f30aa09be54e4a358d95e30772cd8140efc0440d1c39c09ebe7d24a84d298f87/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/f30aa09be54e4a358d95e30772cd8140efc0440d1c39c09ebe7d24a84d298f87/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/config/toliss/a320-332a-ee23.json`
- Archive paths: mac: `00_ToLissOANS/config/toliss/a320-332a-ee23.json`

### `00_ToLissOANS/config/toliss/a321.json`

- SHA-256: `a2adfbfd63279fe378cdd8d08b85f5c2e2c9679547616c93383e76b9f15aa3b0`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/a2adfbfd63279fe378cdd8d08b85f5c2e2c9679547616c93383e76b9f15aa3b0/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/a2adfbfd63279fe378cdd8d08b85f5c2e2c9679547616c93383e76b9f15aa3b0/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=48, 超时=11, 确认超时=0, 失败=2, 不支持=14
- Results: malicious=0, suspicious=0, undetected=48, timeout=11, confirmed-timeout=0, failure=2, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/config/toliss/a321.json`; win: `00_ToLissOANS/config/toliss/a21n.json`; win: `00_ToLissOANS-debug/config/toliss/a321.json`; win: `00_ToLissOANS-debug/config/toliss/a21n.json`; mac: `00_ToLissOANS/config/toliss/a321.json`; mac: `00_ToLissOANS/config/toliss/a21n.json`
- Archive paths: win: `00_ToLissOANS/config/toliss/a321.json`; win: `00_ToLissOANS/config/toliss/a21n.json`; win: `00_ToLissOANS-debug/config/toliss/a321.json`; win: `00_ToLissOANS-debug/config/toliss/a21n.json`; mac: `00_ToLissOANS/config/toliss/a321.json`; mac: `00_ToLissOANS/config/toliss/a21n.json`

### `00_ToLissOANS/config/toliss/a339.json`

- SHA-256: `f2aea155bc2047fe82de40676eff332337f2a71754ba59ba02bbfce8912c5bcc`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/f2aea155bc2047fe82de40676eff332337f2a71754ba59ba02bbfce8912c5bcc/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/f2aea155bc2047fe82de40676eff332337f2a71754ba59ba02bbfce8912c5bcc/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/config/toliss/a339.json`; win: `00_ToLissOANS-debug/config/toliss/a339.json`; mac: `00_ToLissOANS/config/toliss/a339.json`
- Archive paths: win: `00_ToLissOANS/config/toliss/a339.json`; win: `00_ToLissOANS-debug/config/toliss/a339.json`; mac: `00_ToLissOANS/config/toliss/a339.json`

### `00_ToLissOANS/config/toliss/a346.json`

- SHA-256: `8801921e62d47f1ba19ffa2b121c979dc84cf6153ffafed0d5a7d484f4d94395`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/8801921e62d47f1ba19ffa2b121c979dc84cf6153ffafed0d5a7d484f4d94395/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/8801921e62d47f1ba19ffa2b121c979dc84cf6153ffafed0d5a7d484f4d94395/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/config/toliss/a346.json`; win: `00_ToLissOANS-debug/config/toliss/a346.json`; mac: `00_ToLissOANS/config/toliss/a346.json`
- Archive paths: win: `00_ToLissOANS/config/toliss/a346.json`; win: `00_ToLissOANS-debug/config/toliss/a346.json`; mac: `00_ToLissOANS/config/toliss/a346.json`

### `00_ToLissOANS/config/zibo/b738.json`

- SHA-256: `02f6b924d43ca739fcd98ac72461b0a5a85e8b175ef22846d3e9100dd1b5b8de`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/02f6b924d43ca739fcd98ac72461b0a5a85e8b175ef22846d3e9100dd1b5b8de/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/02f6b924d43ca739fcd98ac72461b0a5a85e8b175ef22846d3e9100dd1b5b8de/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=59, 超时=1, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=59, timeout=1, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/config/zibo/b738.json`
- Archive paths: mac: `00_ToLissOANS/config/zibo/b738.json`

### `00_ToLissOANS/config/zibo/b738.json`

- SHA-256: `3e60a932f4c04e71c530d936db4a97e42080d7189f19f01da8015cd13a13b367`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/3e60a932f4c04e71c530d936db4a97e42080d7189f19f01da8015cd13a13b367/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/3e60a932f4c04e71c530d936db4a97e42080d7189f19f01da8015cd13a13b367/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/config/zibo/b738.json`; win: `00_ToLissOANS-debug/config/zibo/b738.json`
- Archive paths: win: `00_ToLissOANS/config/zibo/b738.json`; win: `00_ToLissOANS-debug/config/zibo/b738.json`

### `00_ToLissOANS/INSTALL.txt`

- SHA-256: `fd59ff595016f3810e548ecd53bc9b86614773bbd6473949ff41ce7d8431015c`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/fd59ff595016f3810e548ecd53bc9b86614773bbd6473949ff41ce7d8431015c/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/fd59ff595016f3810e548ecd53bc9b86614773bbd6473949ff41ce7d8431015c/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/INSTALL.txt`
- Archive paths: win: `00_ToLissOANS/INSTALL.txt`

### `00_ToLissOANS/LICENSE`

- SHA-256: `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986/detection)
- 报告时间: 2026-11-02 13:09 UTC；重新扫描仍在排队，使用先前报告
- Report date: 2026-11-02 13:09 UTC; new rescan queued, displaying previous report
- 结果: 恶意=0, 可疑=0, 未检出=61, 超时=0, 确认超时=0, 失败=0, 不支持=14
- Results: malicious=0, suspicious=0, undetected=61, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/LICENSE`
- Archive paths: mac: `00_ToLissOANS/LICENSE`

### `00_ToLissOANS/MACOS.md`

- SHA-256: `5944c9a1c216e47d29d86c305ed5f48451ef5359cbadf5467d8fac6650006975`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/5944c9a1c216e47d29d86c305ed5f48451ef5359cbadf5467d8fac6650006975/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/5944c9a1c216e47d29d86c305ed5f48451ef5359cbadf5467d8fac6650006975/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=60, 超时=0, 确认超时=0, 失败=0, 不支持=15
- Results: malicious=0, suspicious=0, undetected=60, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=15
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/MACOS.md`
- Archive paths: mac: `00_ToLissOANS/MACOS.md`

### `00_ToLissOANS/manifest.json`

- SHA-256: `b6966195b28fe6157c92b6def5c0e938c56236f6f051288650e36a027660e55c`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/b6966195b28fe6157c92b6def5c0e938c56236f6f051288650e36a027660e55c/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/b6966195b28fe6157c92b6def5c0e938c56236f6f051288650e36a027660e55c/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=61, 超时=0, 确认超时=0, 失败=0, 不支持=14
- Results: malicious=0, suspicious=0, undetected=61, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: win: `00_ToLissOANS/manifest.json`
- Archive paths: win: `00_ToLissOANS/manifest.json`

### `00_ToLissOANS/README.md`

- SHA-256: `c09f035b2d8a44c9631c3dbb5b3e8fc2c558a7dc5df90b8bf0822c95c0d9b746`
- [VirusTotal 报告](https://www.virustotal.com/gui/file/c09f035b2d8a44c9631c3dbb5b3e8fc2c558a7dc5df90b8bf0822c95c0d9b746/detection)
- [VirusTotal report](https://www.virustotal.com/gui/file/c09f035b2d8a44c9631c3dbb5b3e8fc2c558a7dc5df90b8bf0822c95c0d9b746/detection)
- 报告时间: 2026-11-07 15:45 UTC；已完成的报告
- Report date: 2026-11-07 15:45 UTC; completed report
- 结果: 恶意=0, 可疑=0, 未检出=61, 超时=0, 确认超时=0, 失败=0, 不支持=14
- Results: malicious=0, suspicious=0, undetected=61, timeout=0, confirmed-timeout=0, failure=0, type-unsupported=14
- 指定引擎: BitDefender=未检出, Microsoft=未检出, Malwarebytes=未检出
- Named engines: BitDefender=undetected, Microsoft=undetected, Malwarebytes=undetected
- 压缩包内路径: mac: `00_ToLissOANS/README.md`
- Archive paths: mac: `00_ToLissOANS/README.md`

