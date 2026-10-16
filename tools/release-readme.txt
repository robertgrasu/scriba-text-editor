SCRIBA - versiune de test
=========================

Un editor de text pentru Windows.
Nu are instalare: rulezi Scriba.exe din orice folder. Nu cere drepturi de administrator.

1. PORNIRE
   Dai dublu click pe Scriba.exe.
   Windows poate afisa "Windows protected your PC" (SmartScreen), pentru ca programul nu are inca semnatura de editor.
   Apesi "More info", apoi "Run anyway".

2. LICENTA
   Programul nu functioneaza fara licenta. La prima pornire apare ecranul de activare:
   scrii cheia primita (arata ca SCRB-XXXX-XXXX-XXXX-XXXX) si apesi Activate.
   Ai nevoie de internet doar la activare si din cand in cand, in fundal, pentru verificare.
   Licenta se leaga de calculatorul pe care o activezi. Fara conexiune, programul merge pana la 14 zile dupa ultima verificare.
   Starea licentei o vezi oricand la Help > License.

3. UNDE SE PASTREAZA DATELE
   %APPDATA%\scriba-cpp  (setari, notele, sesiunea; tot ce scrii nesalvat se pastreaza la inchidere)
   Setarile le deschizi cu Preferences > Settings (Ctrl+,).

4. CE SA INCERCI
   - Ctrl+P  : deschide repede un fisier; Ctrl+Shift+P : toate comenzile
   - File > Open Folder : bara laterala cu fisiere; lupa de langa FOLDERS cauta dupa numele fisierului
   - Ctrl+Shift+F : cauta in toate fisierele
   - Preferences > Settings > Appearance : teme de culori
   - Notes : note rapide, cu Markdown

5. DACA CEVA NU MERGE
   Trimite-mi, din folderul %APPDATA%\scriba-cpp, fisierele crash.txt si scriba.log (daca exista), plus ce faceai in clipa aceea.
   Contin doar date tehnice (adrese, erori, cai de fisiere), nu continutul documentelor tale.

Licentele parti terte folosite sunt in THIRD_PARTY.md si in folderul licenses.
