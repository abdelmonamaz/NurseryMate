# 📋 Nursera — Spécifications fonctionnelles détaillées

> Version 1.0 · 14/07/2026 · Réf. : [00-vision-et-grands-axes.md](00-vision-et-grands-axes.md)
> Convention : chaque exigence est identifiée `Fxx-nn` (module-numéro). Priorités : **[MVP]**, **[V1]**, **[V1.5]**.

---

## Sommaire

- [M01 — Authentification, rôles & permissions](#m01)
- [M02 — Catalogue plantes](#m02)
- [M03 — Stock & emplacements](#m03)
- [M04 — Ventes & caisse](#m04)
- [M05 — Devis & factures](#m05)
- [M06 — Clients](#m06)
- [M07 — Fournisseurs & achats](#m07)
- [M08 — Production & culture](#m08)
- [M09 — Application Android terrain](#m09)
- [M10 — Rapports & tableau de bord](#m10)
- [M11 — Administration & paramètres](#m11)
- [Matrice des permissions](#permissions)
- [Règles transverses](#transverses)

---

<a id="m01"></a>
## M01 — Authentification, rôles & permissions

### Objectif
Contrôler l'accès aux fonctions selon le profil, sans complexifier le quotidien (PME de 2 à 5 utilisateurs).

### User stories
- **US-01.1** — En tant que gérant, je crée des comptes utilisateur avec un rôle, pour que chacun n'accède qu'à ce qui le concerne.
- **US-01.2** — En tant que vendeuse, je me connecte par code PIN à 4-6 chiffres pour ouvrir ma session de caisse en 2 secondes.
- **US-01.3** — En tant qu'ouvrier, je choisis mon nom sur l'écran Android et saisis mon PIN, sans clavier complexe.

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F01-01 | Connexion par sélection d'utilisateur + PIN (4 à 6 chiffres). Mot de passe complet optionnel pour le rôle Gérant. | [MVP] |
| F01-02 | 3 rôles prédéfinis : **Gérant** (tout), **Vendeur** (ventes, clients, consultation stock/catalogue), **Ouvrier** (stock terrain, production, consultation catalogue). | [MVP] |
| F01-03 | Verrouillage de session après N minutes d'inactivité (configurable, défaut 10 min desktop / 5 min Android). | [MVP] |
| F01-04 | Toute écriture (vente, mouvement, perte…) est horodatée et attribuée à l'utilisateur connecté. | [MVP] |
| F01-05 | Blocage après 5 PIN erronés consécutifs ; déblocage par le Gérant. | [V1] |
| F01-06 | Journal d'audit consultable par le Gérant : qui a fait quoi, quand (créations, modifications, suppressions, annulations). | [V1] |

### Règles de gestion
- **RG-01.a** — Il existe toujours au moins un compte Gérant actif ; le dernier ne peut être ni supprimé ni rétrogradé.
- **RG-01.b** — Un utilisateur désactivé ne peut plus se connecter, mais son historique est conservé (pas de suppression physique).

---

<a id="m02"></a>
## M02 — Catalogue plantes

### Objectif
Référentiel unique des produits : plantes, mais aussi articles annexes (pots, terreau, engrais, outils).

### User stories
- **US-02.1** — En tant que gérant, je crée une fiche plante complète en < 2 minutes, avec photo.
- **US-02.2** — En tant que vendeuse, je retrouve une plante par son nom FR, AR ou botanique, en tapant 3 lettres.
- **US-02.3** — En tant qu'ouvrier, je scanne l'étiquette QR d'un plant pour ouvrir sa fiche.

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F02-01 | Fiche produit : nom commercial **FR + AR**, nom botanique (latin), catégorie, description, photos (1 principale + galerie), statut actif/inactif. | [MVP] |
| F02-02 | Catégories hiérarchiques à 2 niveaux (ex. : *Arbres fruitiers > Agrumes*). Arborescence type fournie en annexe A. | [MVP] |
| F02-03 | **Variantes de conditionnement** par plante : godet, pot Ø (10/14/17/21…), motte, racines nues, caisse — chaque variante a son propre prix de vente, coût moyen, code-barres/SKU et stock. | [MVP] |
| F02-04 | Prix de vente TTC en TND (3 décimales), taux de TVA par produit (0/7/13/19 %), prix professionnel optionnel (2e tarif). | [MVP] |
| F02-05 | Attributs horticoles optionnels : exposition (soleil/mi-ombre/ombre), besoin en eau (1-3), rusticité, période de floraison, période de plantation, hauteur adulte. | [V1] |
| F02-06 | Recherche instantanée multilingue (FR/AR/latin) + filtres catégorie, disponibilité, saison. | [MVP] |
| F02-07 | Génération d'étiquettes QR par variante (planche A4 ou rouleau) : nom FR/AR, prix, QR. | [V1] |
| F02-08 | Articles non-plantes (pots, terreau, engrais…) gérés dans le même catalogue, avec un type « article ». | [MVP] |
| F02-09 | Duplication de fiche (« créer à partir de ») pour accélérer la saisie. | [MVP] |
| F02-10 | Import initial CSV (nom, catégorie, prix, stock) pour la reprise de l'existant. | [V1] |

### Règles de gestion
- **RG-02.a** — Un produit référencé dans une vente ou un mouvement ne peut pas être supprimé — seulement désactivé.
- **RG-02.b** — Le SKU est unique ; généré automatiquement (`CAT-NNNN-VAR`) mais modifiable à la création.
- **RG-02.c** — Prix saisi TTC ; le HT est calculé (affiché sur les factures). Arrondi au millime.

---

<a id="m03"></a>
## M03 — Stock & emplacements

### Objectif
Savoir à tout instant **combien** de chaque variante est disponible et **où**.

### User stories
- **US-03.1** — En tant que gérant, je définis mes emplacements (Serre 1, Serre 2, Parcelle A, Zone de vente) une fois pour toutes.
- **US-03.2** — En tant qu'ouvrier, je déplace 50 plants de la Serre 1 vers la Zone de vente en 4 taps sur Android.
- **US-03.3** — En tant que gérant, je reçois une alerte quand le stock d'un produit passe sous son seuil.
- **US-03.4** — En tant qu'ouvrier, je fais l'inventaire d'une serre : l'app me liste ce qu'elle croit présent, je corrige les quantités.

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F03-01 | Emplacements paramétrables : nom FR/AR, type (serre / parcelle / zone de vente / dépôt), statut. | [MVP] |
| F03-02 | Stock tenu **par variante × emplacement**. Vue agrégée par produit. | [MVP] |
| F03-03 | Types de mouvements : **entrée** (réception, production), **sortie** (vente, perte, don, usage interne), **transfert** (entre emplacements), **ajustement** (inventaire). Tout mouvement porte : date, utilisateur, quantité, motif, commentaire optionnel. | [MVP] |
| F03-04 | Les ventes (M04) et réceptions d'achat (M07) génèrent automatiquement les mouvements de stock. | [MVP] |
| F03-05 | Seuil d'alerte par variante ; badge « stock bas » sur le tableau de bord + liste des produits sous seuil. | [MVP] |
| F03-06 | **Inventaire guidé** par emplacement : liste théorique → saisie du compté → écarts calculés → validation qui génère les ajustements. Inventaire partiel possible (une catégorie, une serre). | [MVP] |
| F03-07 | Historique complet des mouvements, filtrable (produit, emplacement, type, période, utilisateur). | [MVP] |
| F03-08 | Motifs de perte typés : mortalité, casse, maladie, gel, invendable, autre — pour analyse (M10). | [V1] |
| F03-09 | Valorisation du stock au coût moyen pondéré (CMP), recalculé à chaque réception. | [V1] |

### Règles de gestion
- **RG-03.a** — Un mouvement validé ne se modifie pas : il s'annule par un mouvement inverse (traçabilité).
- **RG-03.b** — Le stock peut être temporairement négatif (vente saisie avant la réception) : autorisé mais signalé visuellement ; le Gérant voit la liste des stocks négatifs.
- **RG-03.c** — Un inventaire validé verrouille les quantités comptées à sa date ; les écarts sont journalisés en ajustements distincts.

---

<a id="m04"></a>
## M04 — Ventes & caisse

### Objectif
Encaisser vite au comptoir, tracer 100 % des ventes.

### User stories
- **US-04.1** — En tant que vendeuse, je saisis une vente (3 articles, remise, espèces) en moins de 30 secondes.
- **US-04.2** — En tant que vendeuse, j'encaisse un paiement partiel d'un client professionnel ; le solde passe en créance.
- **US-04.3** — En tant que gérant, je consulte le journal des ventes du jour et le total par mode de paiement.

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F04-01 | Écran de caisse : recherche produit (texte / douchette / grille des favoris), panier, quantités, prix modifiable ligne (droit Vendeur paramétrable), total TTC. | [MVP] |
| F04-02 | Remise par ligne (%) et remise globale (% ou montant). Plafond de remise par rôle (défaut : Vendeur ≤ 10 %, au-delà → validation Gérant). | [MVP] |
| F04-03 | Modes de paiement : espèces (avec calcul du rendu), chèque (n°, banque, échéance), virement, mixte. | [MVP] |
| F04-04 | Vente au comptant anonyme (« client de passage ») ou rattachée à une fiche client. | [MVP] |
| F04-05 | **Paiement partiel / crédit** : uniquement pour un client identifié ; le solde alimente son encours (M06). | [MVP] |
| F04-06 | Ticket de caisse : impression 80 mm et/ou PDF A5 — logo, coordonnées, lignes, TVA récapitulée, total TND. | [MVP] |
| F04-07 | Journal des ventes : liste filtrable (jour, vendeur, mode de paiement, client), totaux. | [MVP] |
| F04-08 | Annulation d'une vente le jour même par le Gérant (motif obligatoire) → contre-mouvements de stock + trace d'audit. Retour/avoir au-delà du jour. | [V1] |
| F04-09 | Clôture de caisse quotidienne : total théorique espèces vs compté, écart journalisé. | [V1] |
| F04-10 | Mise en attente d'un panier (le client ajoute des articles pendant qu'un autre paie). | [V1] |

### Règles de gestion
- **RG-04.a** — Une vente validée décrémente le stock de l'emplacement « Zone de vente » par défaut (emplacement de caisse configurable).
- **RG-04.b** — Numérotation continue des tickets : `T-AAAA-NNNNN`, sans trou, par année.
- **RG-04.c** — Prix pro appliqué automatiquement si le client est de type professionnel (débrayable ligne à ligne).
- **RG-04.d** — Une vente à crédit exige un client identifié dont l'encours ne dépasse pas son plafond (RG-06.b) — sinon validation Gérant.

---

<a id="m05"></a>
## M05 — Devis & factures

### Objectif
Répondre aux clients professionnels (paysagistes, communes, hôtels) avec des documents conformes.

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F05-01 | Devis : lignes produits ou lignes libres (prestation de plantation…), remises, validité (défaut 30 j), PDF bilingue selon la langue du client. | [V1] |
| F05-02 | Statuts devis : brouillon → envoyé → accepté / refusé / expiré. Conversion en vente/facture en 1 clic (reprend lignes et prix). | [V1] |
| F05-03 | Facture conforme Tunisie : n° séquentiel `F-AAAA-NNNNN`, matricule fiscal société et client (si assujetti), détail HT/TVA par taux, **timbre fiscal** (montant paramétrable), total TTC en chiffres et **en lettres (FR ou AR)**. | [V1] |
| F05-04 | Facture générable depuis une vente comptoir (le client demande une facture après coup) ou directement. | [V1] |
| F05-05 | Suivi des factures : payée / partiellement payée / impayée ; relance simple (liste des impayées + ancienneté). | [V1] |
| F05-06 | Bon de livraison optionnel lié à une vente/facture (sans prix, pour l'équipe de livraison). | [V1.5] |

### Règles de gestion
- **RG-05.a** — Une facture émise n'est pas modifiable ni supprimable : correction par **avoir** (`AV-AAAA-NNNNN`).
- **RG-05.b** — Un devis n'impacte jamais le stock ; option « réserver le stock » [V1.5].
- **RG-05.c** — Le timbre fiscal s'applique une fois par facture, hors base TVA, montant défini dans les paramètres (défaut 1,000 TND).

---

<a id="m06"></a>
## M06 — Clients

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F06-01 | Fiche client : type (particulier / professionnel), nom/raison sociale, téléphone(s), email, adresse, matricule fiscal (pro), langue préférée (FR/AR), notes. | [MVP] |
| F06-02 | Historique : ventes, factures, devis, paiements, encours. | [MVP] |
| F06-03 | **Gestion des créances** : encours par client, plafond de crédit optionnel, saisie d'un règlement sur encours (imputation aux plus anciennes échéances). | [MVP] |
| F06-04 | Recherche par nom ou téléphone ; détection de doublon sur le téléphone à la création. | [MVP] |
| F06-05 | Liste des créances triée par ancienneté (balance âgée simple : < 30 j, 30-90 j, > 90 j). | [V1] |

### Règles de gestion
- **RG-06.a** — Un client avec historique ne se supprime pas ; il se désactive.
- **RG-06.b** — Si un plafond de crédit est défini, toute vente à crédit qui le dépasse exige la validation du Gérant.

---

<a id="m07"></a>
## M07 — Fournisseurs & achats

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F07-01 | Fiche fournisseur : raison sociale, contacts, matricule fiscal, conditions de paiement, notes. | [V1] |
| F07-02 | Commande d'achat : lignes variantes + prix d'achat, statuts brouillon → envoyée → reçue (totale/partielle) → clôturée. | [V1] |
| F07-03 | **Réception** : contrôle des quantités reçues vs commandées, génère les entrées de stock sur l'emplacement choisi et met à jour le coût moyen (CMP). | [V1] |
| F07-04 | Réception directe sans commande préalable (achat au marché) : saisie rapide fournisseur + lignes + coûts. | [V1] |
| F07-05 | Suivi des paiements fournisseurs (dettes), échéances chèques. | [V1.5] |

---

<a id="m08"></a>
## M08 — Production & culture

### Objectif
Tracer le cycle de production interne : de la bouture/semence au plant vendable, et mesurer les pertes.

### User stories
- **US-08.1** — En tant que gérant, je crée un lot « 500 boutures de romarin, Serre 2, semaine 12 » et je suis son évolution.
- **US-08.2** — En tant qu'ouvrier, je déclare : « 300 plants du lot L-2026-041 rempotés en pot Ø14, 20 perdus ».
- **US-08.3** — En tant que gérant, je vois le taux de réussite par espèce et par saison.

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F08-01 | **Lot de production** : n° auto (`L-AAAA-NNN`), produit cible, origine (semis / bouturage / division / achat jeune plant), quantité initiale, date, emplacement, notes. | [V1] |
| F08-02 | Événements de lot : rempotage (change la variante et/ou l'emplacement), déplacement, perte (motif typé), passage en « vendable » (bascule le lot dans le stock commercial). | [V1] |
| F08-03 | Traitements & interventions : arrosage exceptionnel, fertilisation, traitement phyto (produit utilisé, dose), taille — journal par lot. | [V1.5] |
| F08-04 | Rappels d'entretien planifiables (ex. « fertiliser Serre 1 dans 15 j ») affichés au tableau de bord et sur Android. | [V1.5] |
| F08-05 | Indicateurs par lot : taux de survie, durée de cycle, coût estimé. | [V1.5] |

### Règles de gestion
- **RG-08.a** — Tant qu'un lot n'est pas « vendable », ses quantités apparaissent dans le **stock de production**, distinct du stock commercial (non vendable en caisse).
- **RG-08.b** — La somme (vendable + pertes + restant en production) reste égale à la quantité initiale du lot — l'app garantit la cohérence.

---

<a id="m09"></a>
## M09 — Application Android terrain

### Objectif
Compagnon de terrain **offline-first**, en 3 taps maximum par action, gros boutons, arabe par défaut si l'utilisateur le choisit.

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F09-01 | Fonctionne **sans réseau** : catalogue, stock et saisies en local ; synchronisation automatique au retour du Wi-Fi + bouton « Synchroniser maintenant ». Indicateur clair de l'état de sync (à jour / N saisies en attente / erreur). | [MVP] |
| F09-02 | Accueil = 4 grosses tuiles : **Chercher un plant**, **Inventaire**, **Déplacer**, **Déclarer une perte** (+ « Lot » en V1). | [MVP] |
| F09-03 | Consultation produit : photo, prix, stock par emplacement. | [MVP] |
| F09-04 | Scan QR/code-barres via caméra pour ouvrir une fiche ou ajouter à un inventaire. | [V1] |
| F09-05 | Inventaire guidé par emplacement (cf. F03-06), utilisable hors ligne, envoyé à la validation. | [MVP] |
| F09-06 | Prise de photo terrain rattachée à un produit ou un lot (état sanitaire, croissance). | [V1] |
| F09-07 | Saisie production : rempotage, perte, déplacement de lot (cf. M08). | [V1] |
| F09-08 | Mini tableau de bord lecture seule pour le Gérant (ventes du jour, alertes stock). | [V1] |
| F09-09 | Appairage initial avec le poste serveur par scan d'un QR affiché sur le desktop (adresse + jeton). | [MVP] |

### Règles de gestion
- **RG-09.a** — Les saisies hors-ligne sont horodatées à la saisie (pas à la sync) et conservées jusqu'à acquittement du serveur.
- **RG-09.b** — En conflit (ex. deux inventaires du même emplacement), la résolution suit les règles de sync (voir spéc. technique §6) ; les cas non auto-résolubles remontent au Gérant dans une file « Conflits à arbitrer ».

---

<a id="m10"></a>
## M10 — Rapports & tableau de bord

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F10-01 | **Tableau de bord** desktop : CA du jour / de la semaine / du mois, nombre de ventes, panier moyen, top 5 produits, alertes stock bas, créances totales, saisies terrain en attente de sync. | [MVP] |
| F10-02 | Rapport des ventes : par période, catégorie, produit, vendeur, mode de paiement, type de client. Comparaison N vs N-1. | [V1] |
| F10-03 | Rapport de stock : état valorisé (CMP), rotation, produits dormants (aucune vente depuis N mois). | [V1] |
| F10-04 | Rapport des pertes : quantités et valeur par motif, par catégorie, par emplacement, par période. | [V1] |
| F10-05 | Rapport de marge : marge par produit/catégorie (prix de vente vs CMP). | [V1.5] |
| F10-06 | Export : PDF (mise en page rapport) et CSV (données brutes) pour tous les rapports. | [V1] |
| F10-07 | Saisonnalité : ventes par mois sur 12-24 mois glissants, par catégorie (préparer les productions). | [V1.5] |

---

<a id="m11"></a>
## M11 — Administration & paramètres

### Exigences
| ID | Exigence | Priorité |
|---|---|---|
| F11-01 | Paramètres société : nom, logo (fourni), adresse, téléphones, email, matricule fiscal, RC, pied de page documents FR/AR. | [MVP] |
| F11-02 | Paramètres financiers : devise TND (fixe), taux de TVA disponibles, timbre fiscal (montant, actif/inactif), plafonds de remise par rôle. | [MVP] |
| F11-03 | Langue de l'interface par utilisateur : FR ou AR (avec RTL complet). Changement à chaud sans redémarrage. | [MVP] |
| F11-04 | **Sauvegarde automatique** quotidienne de la base (locale + dossier/clé USB configurable), rotation sur 30 jours, restauration guidée. Sauvegarde manuelle en 1 clic. | [MVP] |
| F11-05 | Gestion des utilisateurs (cf. M01) et des appareils appairés (liste, révocation d'un Android perdu). | [MVP] |
| F11-06 | Gestion des référentiels : catégories, emplacements, motifs de perte, conditionnements. | [MVP] |
| F11-07 | Numérotations documentaires visibles (prochains n° ticket/facture/avoir) — informatif, non modifiable après émission. | [V1] |

---

<a id="permissions"></a>
## Matrice des permissions (par défaut)

| Action | Gérant | Vendeur | Ouvrier |
|---|:---:|:---:|:---:|
| Catalogue — consulter | ✅ | ✅ | ✅ |
| Catalogue — créer/modifier/prix | ✅ | ❌ | ❌ |
| Stock — consulter | ✅ | ✅ | ✅ |
| Stock — mouvements & inventaire | ✅ | ⚙️ | ✅ |
| Vente comptoir | ✅ | ✅ | ❌ |
| Remise au-delà du plafond / annulation / avoir | ✅ | ❌ | ❌ |
| Devis / factures | ✅ | ✅ | ❌ |
| Clients — consulter/créer | ✅ | ✅ | ❌ |
| Créances — encaisser un règlement | ✅ | ✅ | ❌ |
| Fournisseurs & achats | ✅ | ❌ | ❌ |
| Production — consulter | ✅ | ✅ | ✅ |
| Production — saisir (lot, perte, rempotage) | ✅ | ❌ | ✅ |
| Rapports & tableau de bord complet | ✅ | ❌ | ❌ |
| Administration, utilisateurs, sauvegardes | ✅ | ❌ | ❌ |

⚙️ = configurable. Le Gérant peut ajuster chaque ligne par rôle (simple matrice de cases à cocher, pas de rôles personnalisés en V1).

---

<a id="transverses"></a>
## Règles transverses

- **RT-01 · Monnaie** — Tous les montants en TND, **3 décimales** (millimes), arrondi demi-supérieur au millime. Affichage : `12,500 DT` (FR) / `12,500 د.ت` (AR).
- **RT-02 · Dates** — Stockage UTC, affichage heure locale (Africa/Tunis). Format FR `14/07/2026`, AR `2026/07/14` selon la locale.
- **RT-03 · Bilinguisme** — Tout libellé métier saisi par l'utilisateur (produit, catégorie, emplacement) a un champ FR et un champ AR ; si l'un est vide, l'autre est affiché en repli. L'interface (menus, boutons) est traduite intégralement.
- **RT-04 · Suppression** — Jamais de suppression physique d'une entité référencée : désactivation + conservation de l'historique.
- **RT-05 · Performance perçue** — Toute action courante (recherche produit, ajout au panier, validation mouvement) répond en < 200 ms sur le matériel cible.
- **RT-06 · Résilience** — Coupure de courant ou crash en cours de saisie : aucune donnée validée n'est perdue ; un panier de caisse en cours est restauré au redémarrage.

---

## Annexe A — Arborescence de catégories proposée (pré-remplie, modifiable)

- Arbres fruitiers (أشجار مثمرة) : Agrumes, Oliviers, Fruits à noyau, Fruits à pépins, Figuiers & grenadiers
- Arbres & arbustes d'ornement (أشجار وشجيرات الزينة)
- Palmiers & exotiques (نخيل ونباتات استوائية)
- Plantes méditerranéennes & aromatiques (نباتات متوسطية وعطرية)
- Plantes d'intérieur (نباتات داخلية)
- Fleurs saisonnières & vivaces (أزهار موسمية ومعمرة)
- Cactus & succulentes (صبار وعصاريات)
- Plants potagers (شتلات خضروات)
- Gazon & couvre-sols (عشب ومغطيات التربة)
- Fournitures : pots, terreaux, engrais, outillage (مستلزمات)
