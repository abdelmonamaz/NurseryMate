import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Nursera.UI

// Sélecteur de variante réutilisable : recherche (2 lettres min) ->
// suggestions -> chip sélectionnée. `searchFunction` reçoit le terme
// et retourne [{variantId, label, sku, ...}].
ColumnLayout {
    id: picker

    property var searchFunction: null
    property int selectedVariantId: 0
    property string selectedLabel: ""
    property var selectedData: null
    property var suggestions: []
    property alias placeholderText: searchField.placeholderText

    signal picked(var pick)

    function reset() {
        selectedVariantId = 0;
        selectedLabel = "";
        selectedData = null;
        suggestions = [];
        searchField.text = "";
    }

    // Pré-sélection sans recherche (correction guidée, édition) —
    // pick : {variantId, label, …}.
    function preset(pick) {
        selectedData = pick;
        selectedLabel = pick.label;
        selectedVariantId = pick.variantId;
        suggestions = [];
        searchField.text = "";
    }

    spacing: NTheme.s2

    // Chip de sélection
    RowLayout {
        visible: picker.selectedVariantId > 0
        spacing: NTheme.s2

        NBadge {
            text: picker.selectedLabel
            badgeColor: NTheme.primary
        }
        NIconButton {
            text: "✕"
            implicitHeight: 32
            onClicked: picker.reset()
        }
        Item { Layout.fillWidth: true }
    }

    NTextField {
        id: searchField

        Layout.fillWidth: true
        visible: picker.selectedVariantId === 0
        fieldColor: NTheme.surface
        placeholderText: qsTr("Tapez 2 lettres (nom FR/AR, SKU)…")
        onTextEdited: picker.suggestions =
            (text.length >= 2 && picker.searchFunction)
                ? picker.searchFunction(text) : []
    }

    ListView {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(count, 5) * 36
        visible: picker.selectedVariantId === 0 && count > 0
        clip: true
        model: picker.suggestions

        delegate: Rectangle {
            required property var modelData

            width: ListView.view.width
            height: 36
            radius: NTheme.radiusButton
            color: hover.hovered ? NTheme.primaryContainer : "transparent"

            HoverHandler { id: hover }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: NTheme.s2
                text: parent.modelData.label
                      + (parent.modelData.sku ? "  ·  " + parent.modelData.sku : "")
                font.family: NTheme.fontFamily
                font.pixelSize: NTheme.fontSizeBody
                color: NTheme.textPrimary
            }

            TapHandler {
                onTapped: {
                    // selectedData/Label AVANT selectedVariantId : les pages
                    // réagissent sur onSelectedVariantIdChanged et lisent
                    // selectedData à ce moment-là.
                    picker.selectedData = modelData;
                    picker.selectedLabel = modelData.label;
                    picker.selectedVariantId = modelData.variantId;
                    picker.suggestions = [];
                    picker.picked(modelData);
                }
            }
        }
    }
}
