# Changes to the copy of main.cpp for the Pi (the Cardputer project is never
# modified). tools/prepare.sh checks that each one still applies.

# tr() gives the text of the device in use: keyboard, Xbox or PlayStation
# keys (padText, in bridge.inc; user's choice)
s/^const char \*tr(Tx &t) {$/const char *padText(Tx \&t, int l);\n\nconst char *tr(Tx \&t) {/
s/^    return t\[lang\];$/    return padText(t, lang);/
s/^    return multiMode ? multi\[lang\] : solo\[lang\];$/    return tr(multiMode ? multi : solo);/
