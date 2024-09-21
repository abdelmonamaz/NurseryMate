# 🎨 Nursera — Charte UX/UI & Design system

> Version 1.0 · 14/07/2026 · Réf. : [01-specifications-fonctionnelles.md](01-specifications-fonctionnelles.md)
> Implémentation : module QML `ui-common` (thème + composants), cf. [02-specifications-techniques.md](02-specifications-techniques.md) §9.

---

## 1. Principes UX

1. **La caisse d'abord** — l'écran de vente est utilisé 50 fois par jour : zéro friction, tout au clavier possible (desktop), douchette plug-and-play.
2. **Le terrain a des gants** — sur Android : cibles tactiles ≥ 56 dp, une action par écran, jamais plus de 3 taps pour une saisie courante, contrastes élevés (lisible au soleil).
3. **Bilingue natif, pas traduit après coup** — chaque écran est conçu et revu en FR **et** en AR (RTL) ; les maquettes existent dans les deux sens.
4. **Le réseau est un bonus** — l'état de synchronisation est toujours visible mais jamais bloquant ; aucune saisie terrain n'exige le réseau.
5. **Confiance par la traçabilité** — chaque chiffre affiché peut s'expliquer : cliquer un stock montre les mouvements qui y mènent.
6. **Pardonner les erreurs** — annulation par action inverse tracée, confirmations uniquement pour l'irréversible (validation d'inventaire, annulation de vente), jamais pour le courant.

## 2. Identité visuelle

Client : **Pépinière Idéale** — *Vente, Aménagement & Entretien*. Le logo ([logo.jpeg](../logo.jpeg), 1764×1764) est la référence : feuille blanche dans un cercle en dégradé vert anis → vert foncé, texte anthracite + vert.

### 2.0 Couleurs extraites du logo (échantillonnage du fichier)

| Élément du logo | Hex mesuré |
|---|---|
| Haut du dégradé (vert anis) | `#C1CF2C` |
| Feuille / « IDÉALE » (vert clair) | `#85C13A` |
| Milieu du dégradé | `#7A9A28` |
| Bas du dégradé (vert foncé) | `#356A10` |
| « PÉPINIÉRE » / tagline (anthracite) | `#45454A` |

### 2.1 Palette applicative

Le vert anis du logo est trop clair pour du texte ou des boutons sur fond blanc (contraste < 3:1) : il sert d'**accent décoratif**, tandis que le vert foncé du dégradé devient la couleur d'action.

| Jeton | Hex | Usage |
|---|---|---|
| `primary` | `#3E7D14` | Actions principales, éléments actifs (vert foncé du dégradé, AA sur blanc) |
| `primaryDark` | `#2C5E0E` | Hover/pressed, en-têtes |
| `primaryContainer` | `#E9F3DC` | Fonds de sélection, chips |
| `accent` | `#C1CF2C` | Vert anis du logo : surlignages, jauges, décor — **jamais du texte sur blanc** |
| `leaf` | `#85C13A` | Badges positifs, hover, illustrations (vert feuille du logo) |
| `surface` | `#FAFBF7` | Fond général (blanc cassé) |
| `surfaceCard` | `#FFFFFF` | Cartes, panneaux |
| `outline` | `#D9DDD2` | Bordures, séparateurs |
| `textPrimary` | `#45454A` | Texte principal (anthracite du logo) |
| `textSecondary` | `#71767C` | Texte secondaire (tagline du logo) |
| `success` | `#3E7D14` | Confirmations, stock OK |
| `warning` | `#B7791F` | Stock bas, sync en attente |
| `danger` | `#B3261E` | Erreurs, pertes, suppressions |
| `info` | `#2B6CB0` | Informations, badges neutres |

En-têtes de documents (factures, tickets) : logo sur fond blanc uniquement (le JPEG n'a pas de transparence) ; prévoir une déclinaison PNG détourée + une version monochrome pour les tickets thermiques.

Mode sombre : non requis en V1 (usage bureau/jour) — les jetons sont prévus pour l'ajouter en V2 sans refonte.

### 2.2 Typographie

| Usage | Police | Raison |
|---|---|---|
| Latin (FR) | **Inter** (embarquée) | Lisible petites tailles, chiffres tabulaires — les TTF sont déjà dans `GestionScolaire_QML/qml_project/fonts/` (réutilisables tels quels) |
| Arabe | **Cairo** ou **Noto Kufi Arabic** (embarquée) | Moderne, très lisible, bonne harmonie avec Inter |
| Montants & SKU | Inter avec `font-feature: tnum` | Alignement des colonnes de chiffres |

Échelle : 12 / 14 (corps) / 16 / 20 (titres section) / 28 (KPI dashboard) / 40 (total caisse).

### 2.3 Iconographie & style

- Jeu d'icônes outline cohérent (Material Symbols ou Lucide, embarqué en SVG), trait 2 px, remplissage à l'état actif.
- Rayon des cartes 12 px, boutons 8 px ; élévation par ombre douce unique (pas d'empilement d'ombres).
- Illustrations d'états vides simples, thème végétal, une par module (pot vide pour stock vide, etc.).

## 3. Design system — composants QML (`ui-common`)

| Composant | Description |
|---|---|
| `NTheme` (singleton) | Tous les jetons (couleurs, espacements 4/8/12/16/24, tailles, durées d'animation 150 ms) |
| `NButton` | Variantes : primary / secondary / ghost / danger ; tailles M (40) et L (56, terrain) |
| `NTextField`, `NSearchField` | Avec états erreur + libellé flottant ; clavier numérique auto pour quantités/PIN |
| `NCard`, `NListRow` | Carte standard, ligne de liste avec leading/trailing sémantiques (RTL-safe) |
| `NBadge` | Statuts colorés : stock bas, impayé, sync en attente… |
| `NMoneyText` | Affichage montant via `Money` (jamais de formatage à la main en QML) |
| `NQuantityStepper` | − / valeur / + géant pour le terrain |
| `NEmptyState` | Illustration + message + action |
| `NDialog`, `NToast` | Confirmations et feedbacks non bloquants (toast 3 s + action Annuler quand possible) |
| `NSyncIndicator` | Pastille d'état : ✅ à jour / 🟡 N en attente / 🔴 erreur, avec détail au tap |
| `NPinPad` | Pavé PIN plein écran (login) |

**Règle RTL** : tout composant de `ui-common` est développé et testé dans les deux directions ; interdiction de `anchors.left`/`right` dans les écrans métier — uniquement `Layout`, `RowLayout` (qui se miroite) et les slots leading/trailing des composants.

## 4. Navigation & structure des écrans

### 4.1 Desktop — coque applicative

```
┌──────────┬──────────────────────────────────────────────┐
│  LOGO    │  Recherche globale (Ctrl+K)      🔔  👤 Sami │
│──────────┼──────────────────────────────────────────────│
│ ⌂ Accueil│                                              │
│ 🛒 Caisse │                                              │
│ 🌿 Catalog│              ZONE DE CONTENU                 │
│ 📦 Stock  │        (liste ⇄ détail, onglets)             │
│ 🧾 Docs   │                                              │
│ 👥 Clients│                                              │
│ 🚚 Achats │                                              │
│ 🌱 Product│                                              │
│ 📊 Rapport│                                              │
│ ⚙ Réglage│                                              │
├──────────┴──────────────────────────────────────────────┤
│  Poste principal ✅ · 2 appareils connectés · Sync ✅     │
└──────────────────────────────────────────────────────────┘
```

- Sidebar filtrée selon le rôle (le Vendeur voit Caisse, Catalogue, Stock, Docs, Clients).
- En AR : la sidebar passe à droite automatiquement (miroir complet).
- Raccourcis clavier caisse : `F2` nouvelle vente, `F3` recherche produit, `F8` remise, `F10` paiement, `Échap` annuler la ligne.

### 4.2 Écran de caisse (desktop) — wireframe

```
┌─────────────────────────────┬───────────────────────────┐
│ 🔍 Produit / scan…    [F3]  │  VENTE EN COURS   T-2026-… │
│ ┌─────┐ ┌─────┐ ┌─────┐     │  Client : Passage  [+]     │
│ │Citro│ │Rosier│ │Oliv.│    │ ─────────────────────────  │
│ │nnier│ │     │ │     │     │  Citronnier pot21   2 ×    │
│ │12,5 │ │ 8,0 │ │35,0 │     │        25,000  = 50,000    │
│ └─────┘ └─────┘ └─────┘     │  Terreau 50L        1 ×    │
│ ┌─────┐ ┌─────┐ ┌─────┐     │        18,500  = 18,500    │
│ │ …grille des favoris…│     │ ─────────────────────────  │
│ └─────┘ └─────┘ └─────┘     │  Remise [F8]      − 3,500  │
│                             │  TOTAL       65,000 DT     │
│  Catégories : [Agrumes]     │                            │
│  [Aromatiques] [Pots] …     │  [ 💵 PAYER  F10 ]  [⏸ ]   │
└─────────────────────────────┴───────────────────────────┘
```

Écran paiement : gros pavé numérique, boutons Espèces / Chèque / Virement / Mixte, **rendu monnaie affiché en énorme**.

### 4.3 Android — coque applicative

- **Accueil = 4 tuiles pleine largeur** (F09-02) : Chercher 🔍, Inventaire 📋, Déplacer 🔄, Perte ⚠ — plus une barre d'état de sync en haut et le bouton scan flottant.
- Navigation par pile (retour système Android naturel), pas de bottom-nav complexe : chaque tuile ouvre un flux linéaire.
- Toute saisie de quantité passe par `NQuantityStepper` géant + clavier numérique.

### 4.4 Flux terrain « Déplacer » (3 taps + quantité)

```
[Accueil] → tap 1 : tuile Déplacer
   → tap 2 : produit (recherche / scan / récents)
   → tap 3 : Serre 1 ▸ Zone de vente (origine pré-remplie = dernier emplacement utilisé)
   → quantité (stepper) → [VALIDER]
   → toast ✅ « 50 Romarin godet déplacés » + [Annuler]
```

Le même patron s'applique à Perte (motif en gros pictogrammes : 💀 mortalité / 🦠 maladie / ❄ gel / 📦 casse) et aux événements de lot.

### 4.5 Inventaire guidé (Android)

```
┌────────────────────────────┐
│ Inventaire — Serre 1   3/24│   ← progression
│ ┌────────────────────────┐ │
│ │ 📷 Romarin — godet     │ │
│ │ Théorique : 120        │ │
│ │ Compté :  [ 115 ]  −/+ │ │
│ │ [Identique ✓] [Suivant]│ │
│ └────────────────────────┘ │
│  écart affiché en orange   │
└────────────────────────────┘
```

Bouton « Identique ✓ » = 1 tap pour confirmer la quantité théorique (cas majoritaire) ; à la fin, récapitulatif des écarts avant envoi.

## 5. Règles bilinguisme & RTL (rappel design)

- Chaque maquette est validée en FR (LTR) **et** AR (RTL) avant développement ; la revue AR fait partie de la « definition of done » de chaque écran.
- Icônes directionnelles (flèches retour, chevrons) : utiliser les variantes sémantiques miroir ; les icônes symétriques (loupe, corbeille) ne changent pas.
- Chiffres : chiffres arabes occidentaux (0-9) partout, y compris en AR — usage standard en Tunisie pour les montants.
- Champs bilingues (nom FR / nom AR) présentés côte à côte dans les formulaires desktop, avec repli visuel si l'un est vide (RT-03).
- Textes AR : ne jamais tronquer au milieu (élision `…` en début visuel côté approprié), hauteur de ligne +10 % vs latin.

## 6. États, feedback & erreurs

| Situation | Traitement |
|---|---|
| Liste vide | `NEmptyState` : illustration + phrase + bouton d'action (« Ajouter votre première plante ») |
| Chargement | Skeletons (pas de spinner plein écran) ; jamais > 200 ms sans feedback |
| Action réussie | `NToast` succès 3 s, avec **Annuler** quand l'action est réversible |
| Erreur de saisie | Message sous le champ, champ bordé `danger`, focus automatique ; jamais de popup pour une erreur de champ |
| Stock bas / négatif | Badge `warning` / `danger` sur la ligne + compteur sur la sidebar Stock |
| Hors-ligne (Android) | Bandeau discret « Hors ligne — les saisies seront synchronisées », icône `NSyncIndicator` 🟡 |
| Poste principal injoignable (caisse) | Écran pleine page explicite avec diagnostic (« Vérifiez que le PC du bureau est allumé ») |
| Conflit de sync | File « À arbitrer » côté Gérant, comparatif côte à côte, choix explicite |

## 7. Accessibilité & confort

- Contraste AA minimum (4,5:1) sur tout texte ; les statuts ne reposent jamais sur la couleur seule (icône + libellé).
- Desktop : navigation complète au clavier sur la caisse et les formulaires ; ordre de tabulation défini par écran.
- Android : cibles ≥ 56 dp, textes corps ≥ 16 sp, mode « une main » (actions principales en bas d'écran).
- Réglage taille de police (100 % / 115 % / 130 %) dans les paramètres utilisateur.

## 8. Livrables design & processus

| Livrable | Moment |
|---|---|
| Maquettes Figma des 8 écrans clés (caisse, paiement, fiche produit, stock, inventaire mobile, accueil mobile, dashboard, facture PDF) en FR + AR | avant sprint 1 |
| Design system `ui-common` implémenté + galerie de composants (app de démo interne) | sprint 1 |
| Gabarits documents (ticket, facture FR/AR, étiquette QR) validés avec le client | avant V1 |
| Test utilisateur terrain : 1 session avec un ouvrier réel sur le flux inventaire (prototype) | avant fin MVP |
| Revue RTL systématique | à chaque fin de sprint |
