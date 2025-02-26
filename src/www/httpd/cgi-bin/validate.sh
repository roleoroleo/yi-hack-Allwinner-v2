#!/bin/sh

validateFile()
{
    case $1 in
        *[\'!\"@\#\$%\&^*\(\),:\;]* )
            return 1;;
        *)
            return 0;;
    esac
}

validateBaseName()
{
    if [ "${1::1}" == "." ]; then
        return 1
    fi
    case $1 in
        *[\/\'!\"@\#\$%\&^*\(\),:\;]* )
            return 1;;
        *)
            return 0;;
    esac
}

validateDir()
{
    case $1 in
        *[\'!\"@\#\$%^*\(\)_+.,:\;]* )
            return 1;;
        *)
            return 0;;
    esac
}

validateLang()
{
    if [ "${#1}" != "5" ]; then
        return 1
    else
        RES=$(echo ${1} | sed -E 's/[a-zA-Z-]//g')
        if [ -z "$RES" ]; then
            return 0
        else
            return 1
        fi
    fi
}

# Integer and float with . or ,
validateNumber(){
    case $1 in
        ''|*[!0-9.,\-]* )
            return 1;;
        * )
            return 0;;
    esac
}

# Only chars, ditigs and some special chars: '_' '-' ' '
validateString()
{
    case $1 in
        *[\'!\"@\#\$%\&^*\(\).,:\;]* )
            return 1;;
        *)
            return 0;;
    esac
}

validateQueryString()
{
    case $1 in
        *[\'!\"@\#\$%^*\(\),:\;]* )
            return 1;;
        *)
            return 0;;
    esac
}

validateKey()
{
    case $1 in
        *[\|\\\/\'!\"@\#\$%\&^*\(\).,:\;{}=?]* )
            return 1;;
        *)
            return 0;;
    esac
}
