# 📚 Nursera — Documentation produit

Application de gestion pour **Pépinière Idéale** (*Vente, Aménagement & Entretien*, Tunisie) — Desktop + Android, C++/QML/Qt 6.11.

**Assets & références :**
- Logo : [logo.jpeg](../logo.jpeg) (1764×1764) — couleurs extraites dans le doc 03 §2.0 ; prévoir déclinaisons PNG détourée + monochrome.
- Projets de référence de l'auteur (conventions et toolchain éprouvés) :
  - `C:\Users\Azaie\Documents\GestionScolaire_QML\qml_project` — desktop Qt 6.11 MinGW, C++23, pattern Core/Services/Controllers, i18n arabe, Inno Setup
  - `C:\Users\Azaie\Documents\ftour_jomaa` — Android Qt 6.11, kits arm64-v8a/x86_64, SQLite

| # | Document | Contenu | Audience |
|---|---|---|---|
| 00 | [Vision & grands axes](00-vision-et-grands-axes.md) | Contexte, personas, 6 axes produit, roadmap MVP→V2, KPI, contraintes | Tous |
| 01 | [Spécifications fonctionnelles](01-specifications-fonctionnelles.md) | 11 modules détaillés : user stories, exigences priorisées, règles de gestion, matrice des permissions | PO, dev, client |
| 02 | [Spécifications techniques](02-specifications-techniques.md) | Stack Qt 6.8/C++20, architecture en couches, schéma SQLite, API LAN, sync offline-first, sécurité, packaging | Dev |
| 03 | [UX/UI & design system](03-ux-ui-design.md) | Principes UX, palette/typo (FR+AR), composants QML, wireframes, règles RTL | Design, dev |
| 04 | [Schéma BDD](04-schema-bdd.md) | Schéma SQLite implémenté (document vivant) : conventions, ERD, détail des tables, cycle des migrations | Dev |

## Décisions de cadrage (validées le 14/07/2026)

- **Périmètre** : gestion interne complète (catalogue, stock, ventes, clients, achats, production, rapports).
- **Plateformes** : desktop = gestion & caisse · Android = compagnon terrain (inventaire, mouvements, offline-first).
- **Données** : multi-postes sur réseau local, le PC principal héberge la base (pas de cloud, pas d'Internet requis).
- **Langues** : français + arabe, RTL complet.

## Prochaines étapes suggérées

1. Valider les documents avec le gérant (surtout la matrice des permissions, la roadmap MVP, et la part de l'activité *aménagement & entretien* dans le CA — cf. doc 00 §7 V2).
2. ~~Extraire la palette du logo~~ ✅ fait (doc 03 §2.0-2.1) — reste à produire les déclinaisons PNG détourée + monochrome du logo.
3. Maquettes Figma des 8 écrans clés en FR + AR (doc 03 §8).
4. Initialiser le dépôt : squelette CMake calqué sur GestionScolaire + module `ui-common` + migrations SQLite (doc 02 §9).
