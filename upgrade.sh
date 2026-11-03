#!/bin/sh
#
# Safe data-file upgrade for an existing SklaffKOM installation.
#
# This script deliberately does NOT remove or recreate databases, users,
# mailboxes, conference data, local Z-machine games, or other runtime data.
# Run it from the SklaffKOM source tree, normally through "make upgrade".
#

set -eu

SKLAFFDIR=${SKLAFFDIR:-/usr/local/sklaff}
SKLAFFUSER=${SKLAFFUSER:-sklaff}

say()
{
    printf '%s\n' "$*"
}

die()
{
    printf 'upgrade: %s\n' "$*" >&2
    exit 1
}

install_file()
{
    src=$1
    dst=$2
    mode=${3:-600}

    cp "$src" "$dst"
    chown "$SKLAFFUSER" "$dst"
    chmod "$mode" "$dst"
}

ensure_dir()
{
    dir=$1
    mode=$2

    if [ ! -d "$dir" ]; then
        mkdir -p "$dir"
        chown "$SKLAFFUSER" "$dir"
        chmod "$mode" "$dir"
        say "Created $dir"
    fi
}

update_parse()
{
    name=$1
    src="etc/$name"
    dst="$SKLAFFDIR/etc/$name"

    [ -f "$src" ] || die "source file $src is missing"

    if [ ! -e "$dst" ]; then
        install_file "$src" "$dst"
        say "Installed $dst"
        return
    fi

    [ -f "$dst" ] || die "$dst exists but is not a regular file"

    if cmp -s "$src" "$dst"; then
        say "$name is already up to date."
        return
    fi

    stamp=$(date '+%Y%m%d-%H%M%S')
    backup="${dst}.pre-upgrade-${stamp}"
    if [ -e "$backup" ]; then
        backup="${backup}.$$"
    fi

    cp -p "$dst" "$backup"
    install_file "$src" "$dst"

    say "Updated $dst"
    say "  Existing file differed from the distributed version."
    say "  Previous file saved as: $backup"
    say "  Merge any local changes back manually if required."
}

install_zgames()
{
    src="etc/zgames"
    dst="$SKLAFFDIR/etc/zgames"
    zork1_entry='zork 1:Zork I: The Great Underground Empire:zork1.z3'

    [ -f "$src" ] || return

    if [ ! -e "$dst" ]; then
        install_file "$src" "$dst"
        say "Installed $dst"
        return
    fi

    [ -f "$dst" ] || die "$dst exists but is not a regular file"

    if cmp -s "$src" "$dst"; then
        say "zgames is already up to date."
    else
        say "Keeping existing $dst"
        say "  It differs from the distributed etc/zgames template."
        say "  Compare the files manually if you want to merge new examples."

        if ! grep -q '^zork 1:' "$dst"; then
            printf '\n%s\n' "$zork1_entry" >> "$dst"
            say "Added Zork I to $dst"
        fi
    fi
}

install_bundled_zork1()
{
    srcdir="games/zmachine"
    dstdir="$SKLAFFDIR/doors/zmachine"
    story_src="$srcdir/zork1.z3"
    story_dst="$dstdir/zork1.z3"

    [ -f "$story_src" ] || die "source file $story_src is missing"
    [ -f "$srcdir/LICENSE.zork1" ] || die "source file $srcdir/LICENSE.zork1 is missing"
    [ -f "$srcdir/README.zork1" ] || die "source file $srcdir/README.zork1 is missing"

    if [ ! -e "$story_dst" ]; then
        install_file "$story_src" "$story_dst" 644
        say "Installed bundled Zork I as $story_dst"
    else
        [ -f "$story_dst" ] || die "$story_dst exists but is not a regular file"

        if cmp -s "$story_src" "$story_dst"; then
            say "Bundled Zork I is already installed."
        else
            say "Keeping existing $story_dst"
            say "  It differs from the bundled Zork I story file."
            say "  Bundled Zork I metadata will not replace local metadata."
            return
        fi
    fi

    for name in LICENSE.zork1 README.zork1; do
        src="$srcdir/$name"
        dst="$dstdir/$name"

        if [ -f "$dst" ] && cmp -s "$src" "$dst"; then
            continue
        fi

        install_file "$src" "$dst" 644
        say "Updated Zork I file $dst"
    done
}

install_new_etc_files()
{
    for src in etc/*; do
        [ -f "$src" ] || continue

        name=${src#etc/}
        case "$name" in
            parse.swe|parse.eng|zgames)
                continue
                ;;
        esac

        dst="$SKLAFFDIR/etc/$name"
        if [ ! -e "$dst" ]; then
            install_file "$src" "$dst"
            say "Installed new data file $dst"
        fi
    done
}

update_help_dir()
{
    lang=$1
    srcdir="etc/help.$lang"
    dstdir="$SKLAFFDIR/etc/help.$lang"

    [ -d "$srcdir" ] || die "source directory $srcdir is missing"
    ensure_dir "$dstdir" 700

    for src in "$srcdir"/*; do
        [ -f "$src" ] || continue
        name=${src##*/}
        dst="$dstdir/$name"

        if [ -f "$dst" ] && cmp -s "$src" "$dst"; then
            continue
        fi

        install_file "$src" "$dst"
        say "Updated help file $dst"
    done
}

# This target is for an existing installation, not a fresh install.
[ "$(id -u)" -eq 0 ] || die "must be run as root (for example: sudo make upgrade)"
id "$SKLAFFUSER" >/dev/null 2>&1 || die "user '$SKLAFFUSER' does not exist"

[ -d etc ] || die "run this script from the SklaffKOM source directory"
[ -d "$SKLAFFDIR/etc" ] || die "$SKLAFFDIR/etc does not exist; use installdb for a fresh installation"

say "Upgrading SklaffKOM data files in $SKLAFFDIR"
say "No databases or user data will be removed."
say ""

# Runtime directories added over time. Existing directories are left alone;
# only missing directories are created and assigned their normal permissions.
ensure_dir "$SKLAFFDIR/ftnqueue" 700
ensure_dir "$SKLAFFDIR/ftnqueue/tmp" 700
ensure_dir "$SKLAFFDIR/ftnqueue/pending" 700
ensure_dir "$SKLAFFDIR/ftnqueue/netmail" 700
ensure_dir "$SKLAFFDIR/ftnqueue/netmail/tmp" 700
ensure_dir "$SKLAFFDIR/ftnqueue/netmail/pending" 700

ensure_dir "$SKLAFFDIR/doors" 755
ensure_dir "$SKLAFFDIR/doors/zmachine" 755
ensure_dir "$SKLAFFDIR/doors/zmachine/data" 700

ensure_dir "$SKLAFFDIR/etc/help.swe" 700
ensure_dir "$SKLAFFDIR/etc/help.eng" 700

say ""
install_new_etc_files
update_parse parse.swe
update_parse parse.eng
install_bundled_zork1
install_zgames
update_help_dir swe
update_help_dir eng

say ""
say "SklaffKOM data-file upgrade complete."
say "Review any *.pre-upgrade-* files reported above before removing them."
