# H-Bridge proof of concept

Minimale hard- & software + stappenplan dat aantoont dat 2 motoren onafhankelijk van elkaar kunnen draaien, en (traploos) regelbaar zijn in snelheid en draairichting.
De H-brug gebruikt een DRV8833-chip.

## Stappenplan
1. Verbind de ingangen van de chip aan de digitale ingangen van de microcontroller.
2. Verbind de uitgangen van de chip aan de 2 DC-motoren.
3. VCC verbind je met de batterij.
4. EEP verbind je aan de VCC van de microcontroller zodat de chip niet in sleepmodus staat.
5. Verbind ULT met een pull-upweerstand aan VCC. Deze pin wordt gebruikt voor foutdetectie.
