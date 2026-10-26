# Xenia Edge pentru Xbox Series X|S (Developer Mode)

Port UWP experimental al Xenia Edge. Consola trebuie să fie în **Developer Mode**.

## Ce conține arhiva

- `XeniaEdge_<versiune>_x64.msix`: aplicația
- `Dependencies\x64\Microsoft.VCLibs.x64.14.00.appx`: runtime-ul C++ de care are nevoie aplicația
- `XeniaEdge.cer`: certificatul cu care e semnat pachetul (pe Xbox nu e nevoie de el)

## Instalare

1. Pe Xbox, în Dev Home, notează adresa **Remote Access** (de exemplu `https://192.168.1.133:11443`) și deschide-o în browser pe PC.
2. **Add** → alege fișierul `.msix` → **Next**.
3. La dependențe, adaugă `Microsoft.VCLibs.x64.14.00.appx` din folderul `Dependencies\x64` → **Start**.
4. În Dev Home, selectează **Xenia Edge** → butonul **View** (cu trei linii) → **View details** → schimbă **App type** din **App** în **Game**. Fără asta, aplicația primește mult mai puțină memorie și putere de procesare.
5. Tot acolo, la **File system access**, permite accesul, ca aplicația să poată citi jocurile de pe un hard disk extern.

## Jocurile

- Le poți pune pe un hard disk sau stick USB, apoi în aplicație alegi **Add a game folder...** și selectezi folderul.
- Sau le poți copia prin Device Portal (File explorer) în
  `LocalAppData\XeniaEdge.UWP_<id>\LocalState\games`.

Merg folderele extrase (cu `default.xex`), imaginile `.iso`, `.zar` și pachetele Games on Demand.

## Unde găsești log-ul și setările

În Device Portal → **File explorer** → `LocalAppData\XeniaEdge.UWP_<id>\LocalState\`:

- `xenia.log`: log-ul ultimei rulări. Pe el îl trimiți când ceva nu merge.
- `xenia-edge.config.toml`: setările
- `config\<TitleID>.config.toml`: setări pentru un singur joc

## Limitări cunoscute

- Pe Xbox, aplicațiile UWP au doar Direct3D 12 cu **Shader Model 6.4**. Edge folosește acum
  shadere care cer **Shader Model 6.6**, așa că până când partea de shadere e adaptată (vezi
  `tools/uwp/README.md`), jocurile pot rula fără imagine.
- Butonul Xbox (Guide) nu ajunge la joc. Din joc ieși cu butonul Xbox → închizi aplicația.
