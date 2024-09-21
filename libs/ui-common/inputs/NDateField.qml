import QtQuick

// Champ date ISO (AAAA-MM-JJ) : n'accepte que chiffres et tirets,
// bordure rouge tant que la date est incomplète ou impossible
// (2026-13-40 refusée). `dateValid` inclut le champ vide — mettre
// `required: true` pour exiger une date.
NTextField {
    id: control

    property bool required: false
    readonly property bool dateValid: {
        if (text.length === 0)
            return !required;
        if (!/^\d{4}-\d{2}-\d{2}$/.test(text))
            return false;
        const date = new Date(text);
        return !isNaN(date.getTime())
            && text === Qt.formatDate(date, "yyyy-MM-dd");
    }

    placeholderText: qsTr("AAAA-MM-JJ")
    inputMethodHints: Qt.ImhDate
    validator: RegularExpressionValidator {
        regularExpression: /\d{0,4}(-\d{0,2}){0,2}/
    }

    background: Rectangle {
        radius: NTheme.radiusButton
        color: control.enabled ? control.fieldColor : NTheme.surface
        border.width: control.activeFocus || !control.dateValid ? 2 : 1
        border.color: !control.dateValid ? NTheme.danger
            : control.activeFocus ? NTheme.primary : NTheme.outline
    }
}
