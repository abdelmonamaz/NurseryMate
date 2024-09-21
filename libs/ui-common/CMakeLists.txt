# nursera_ui : design system QML (doc 03 §3) — boîte à outils Nursera.UI.
# Rangée par familles : controls/ (boutons, listes déroulantes),
# inputs/ (champs), surfaces/ (cartes, badges, lignes), popups/ (dialogues),
# charts/ (graphiques). Les noms de composants restent inchangés à l'import.

qt_add_library(nursera_ui STATIC)

set_source_files_properties(NTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(nursera_ui
    URI Nursera.UI
    VERSION 1.0
    QML_FILES
        NTheme.qml

        controls/NButton.qml
        controls/NIconButton.qml
        controls/NComboBox.qml

        inputs/NTextField.qml
        inputs/NDateField.qml
        inputs/NFieldLabel.qml

        surfaces/NCard.qml
        surfaces/NBadge.qml
        surfaces/NListRow.qml
        surfaces/NEmptyState.qml

        popups/NDialog.qml
        popups/NPopup.qml

        charts/NLineChart.qml
        charts/NBarChart.qml
)

target_link_libraries(nursera_ui PRIVATE Qt6::Quick)
