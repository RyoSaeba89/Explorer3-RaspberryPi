# Mission control's texts, taken from the TX object of the game's web page
# (page.inc): one C line per text, French then English, in the order of
# texts.h. Run by tools/prepare.sh on the Pi, tools/build.sh on Android:
#   awk -f tools/page-texts.awk page.inc > page_texts.inc
# Stops with an error if a text is missing (a newer game changed the page).

function value(part, key,    at, rest, q, end) {
    at = match(part, "[{, ]" key ":['\"]")
    if (at == 0) {
        missing = missing " " key
        return ""
    }
    rest = substr(part, at + RLENGTH - 1)
    q = substr(rest, 1, 1)
    end = index(substr(rest, 2), q)
    return substr(rest, 2, end - 1)
}

# Item i (0..7) of the array key:['…','…',…]
function item(part, key, i,    at, rest, k, q, end) {
    at = match(part, "[{, ]" key ":\\[")
    if (at == 0) {
        missing = missing " " key
        return ""
    }
    rest = substr(part, at + RLENGTH)
    for (k = 0; k <= i; k++) {
        sub(/^[ ,]*/, "", rest)
        q = substr(rest, 1, 1)
        end = index(substr(rest, 2), q)
        if (k == i)
            return substr(rest, 2, end - 1)
        rest = substr(rest, end + 2)
    }
}

function c(s) {
    gsub(/\\/, "\\\\", s)
    gsub(/"/, "\\\"", s)
    return "\"" s "\""
}

function line(fr, en, key) {
    printf "    {%s, %s},  /* %s */\n", c(fr), c(en), key
}

{ sub(/\r$/, "") }
/^const TX=\{/ { inside = 1 }
inside { tx = tx " " $0 }
inside && /^ *\};?$|\}\};/ { inside = 0 }

END {
    split("cx eq tr r1 r2 r3 r4 t1 p1 q1 t2 p2 t3 p3 q3 t4 p4 h4 p4t e4 ent sor", keys, " ")
    fr = substr(tx, index(tx, "fr:{"), index(tx, " en:{") - index(tx, "fr:{"))
    en = substr(tx, index(tx, " en:{"))
    for (n = 1; n <= 22; n++)
        line(value(fr, keys[n]), value(en, keys[n]), keys[n])
    for (i = 0; i < 8; i++)
        line(item(fr, "pl", i), item(en, "pl", i), "pl[" i "]")
    split("tc pc sa", keys, " ")
    for (n = 1; n <= 3; n++)
        line(value(fr, keys[n]), value(en, keys[n]), keys[n])
    if (tx == "" || missing != "") {
        print "page-texts.awk: not found in the page's TX:" missing > "/dev/stderr"
        exit 1
    }
}
