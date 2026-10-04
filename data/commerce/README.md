# Commerce reference-data design

## Decision

No nationwide `merchants.csv` of real merchants. None exists today:
`merchants.csv` is a legacy standard-exporter table stem, and the
catalogue comes from `synth::merchants::makeCatalog`. The target is
hybrid: pin public aggregates on population and establishments, synthesize
organizations, outlets and remote endpoints from them deterministically,
and keep identities synthetic and seed-replayable. This covers the long
tail without the licensing, privacy, survivorship or update problems of a
real-business list.

The IBM benchmark supports the hierarchy: its
[released TabFormer corpus](https://github.com/IBM/TabFormer) references
100,343 merchant ids, and its
[virtual-world generator paper](https://arxiv.org/abs/1910.03033)
describes 300+ multinational organizations and 16+ million locations.
Reproduce that structure and supply, not the identities.

## Current gap

Categories are chosen uniformly with lognormal popularity, and physical
records go to one of 71 US city rows by population alone. `localOutlet`,
`regionalOutlet` and `nationalService` are distinct enums, but the
card-present pool applies one distance kernel to every located record:
causal geography without empirical local supply.

Since outlets-frequency-2026-09 (authority rows in that amendment of
`docs/fraud_model_audit.md`), every record is an acceptance endpoint, and
`synth::merchants::expandOutlets` expands each physical chain brand (a core
grocery, fuel, restaurant, pharmacy or general-retail record weighing at
least two median stores) into outlets sharing `Record::brand`, each with
its own counterparty account and area. Online records, national services
and the four biller categories stay single endpoints. Visit frequency
follows category (Diary of Consumer Payment Choice rates). Still open:

- outlet counts come from catalogue weights, not CBP cells, and category
  supply is uniform, so the catalogue is sized per customer, not per
  resident (a grocery or restaurant outlet takes about 2,300 of the bank's
  payments a year; a real bank that size would see a few hundred);
- outlets follow national population with no regional clustering, and
  `regionalOutlet` has no kernel of its own;
- omnichannel brands have no remote endpoint;
- `brand` is world state only: the exported `Merchant` id is the endpoint
  and there is no Brand vertex.

The 71 US rows total about 52.9 million residents: selected city cores,
not the US. The geographic catalogue (`data/geo/us_cities.csv`, embedded as
`include/phantomledger/synth/geo/geo_data.hpp`) is a runnable placeholder,
not a Census place or ZCTA extract. It has no land-area column
(`GeoArea.landAreaKm2` is zero), and row position is the `GeoAreaId`, so
inserting a row rekeys later areas. Expanded data needs stable
source-derived keys.

## Pinned public inputs

Normalize only consumed columns. A checked-in manifest records source URL,
vintage, retrieval date, SHA-256, transformation version and license or
public-domain status.

| Model fact | Preferred source | Use |
|---|---|---|
| geographic ids, land area, coordinates | [US Census Gazetteer files](https://www.census.gov/geographies/reference-files/time-series/geo/gazetteer-files.html) | canonical ZCTA/place geometry |
| residential population | [ACS 5-year, B01003](https://api.census.gov/data/2023/acs/acs5/groups.html) | home-area sampling and density |
| establishments by ZIP and NAICS | [County/ZIP Code Business Patterns](https://www.census.gov/programs-surveys/cbp/data/datasets.html) | outlet supply by industry and geography |
| business ZIP to tract/county | [HUD-USPS ZIP Crosswalk](https://www.huduser.gov/portal/datasets/usps_crosswalk.html) | bridge business ZIP mass without treating ZIP as ZCTA |
| firms without employees | [Nonemployer Statistics](https://www.census.gov/data/developers/data-sets/nonemp-api.html) | county-level sole-proprietor long tail |
| in-person vs remote card share | [Federal Reserve Payments Study](https://www.federalreserve.gov/paymentsystems/fr-payments-study.htm) | date-varying channel demand, not outlet supply |

Census demographic products use ZCTAs; business products use USPS ZIP
Codes. They differ. Exact five-digit matches may be used with an explicit
match flag; unmatched business ZIPs fall back through a pinned
ZIP-to-county bridge and are allocated among county ZCTAs. Never claim a
ZBP ZIP is a ZCTA
([Census ZIP/ZCTA guidance](https://www.census.gov/data/what-is-data-census-gov/guidance-for-data-users/frequently-asked-questions/how-can-i-find-data-for-zip-codes-on-data-census-gov.html)).

## Normalized build artifacts

Checked-in CSV, since the tables are flat and projected. Parquet only if a
future panel keeps enough annual/NAICS detail to make CSV size or parse
time matter.

`geo_areas.csv`, one canonical area per row:
`area_key,area_type,source_geoid,state_fips,state_code,latitude_e6,longitude_e6,population,population_moe,land_area_m2,urban_class,gazetteer_vintage,acs_vintage`.
Stable keys such as `US-ZCTA-02138`, canonically sorted before assigning
compact runtime ids. Integer coordinates and square metres keep source
precision; density is derived.

Place identity stays separate:

- `places.csv`: `place_geoid,state_fips,place_name,place_type,latitude_e6,longitude_e6,land_area_m2,population,population_moe,vintage`
- `area_place_links.csv`: `area_key,place_geoid,overlap_weight,is_primary_display_place,assignment_method,quality_flag`

ZCTAs and places overlap many-to-many. Rural or unincorporated areas need
an explicit county-based label. Never copy a ZCTA population into a City
vertex because that City is its display label.

`establishment_cells.csv`, aggregated supply, not identities:
`vintage,business_zip,county_fips,naics,employment_size_class,establishments,employment,annual_payroll,disclosure_code`.
Keep suppression metadata (suppressed is not zero). Allocate ZIP business
mass through a pinned HUD-USPS business-address crosswalk and reconcile to
county CBP totals. Project CBP to the needed NAICS levels before checking
in. Exact ZIP/ZCTA matching is a tagged fallback, never an implicit join.

`merchant_category_crosswalk.csv`, an audited mapping:
`naics,model_category,mcc_family,physical_capable,remote_capable,reach_policy`.
NAICS is the establishment's industry, MCC the acceptance category;
related, not interchangeable. Each mapping is a CHOICE row in
`docs/fraud_model_audit.md` until checked against a publishable network or
issuer crosswalk.

`payment_channel_anchors.csv`, evidence points, not a made-up series:
`year,remote_share_by_count,in_person_chip_share_by_count,population_scope,source_id`.
Interpolate only between documented anchors. Extrapolated years and
pandemic breaks are explicit policy, tested at every anchor.

## World model

Keep three concepts, even in one catalogue: MerchantOrganization (a
synthetic brand/owner with category and size), MerchantOutlet (a physical
location in a canonical area) and MerchantAcceptanceEndpoint (what a
transaction targets: `inPerson` and/or `remote`, optionally an outlet).
Chains own many outlets, independents usually one. Remote endpoints get no
invented location; an omnichannel organization can have both kinds. The
TigerGraph `Merchant` id is the endpoint until Brand/Outlet vertices exist.

Outlet quotas come from establishment cells with deterministic rounding and
named RNG lanes. Supply and popularity are separate: establishment counts
set how many outlets exist, payment-volume/category evidence sets
transaction weights, and neither is uniform by category.

For 1991-2019, organizations and outlets need effective-date intervals
(annual vintages plus a documented entry/survival policy) so no 2024
surface exists unchanged in 1991. NAICS revision bridges and backcasts
before the source panel are CHOICE rows. Coarse yearly birth/death hazards
are acceptable at first, as technical debt until checked against
historical Business Dynamics Statistics and CBP totals. CBP county data
starts in 1986 but ZIP files in 1994, so 1991-1993 needs an explicit
county-to-area allocation or backcast.

## Scalable selection

The `occupiedAreas x physicalMerchants` CDF cannot handle tens of
thousands of ZCTAs. Replace it before expanding:

1. index outlets by area and reach policy;
2. build pools lazily for occupied home areas only;
3. sample from a bounded local-neighbour set (plus an explicit regional or
   national tail), then an outlet within the chosen area;
4. keep separate remote, national-service, regional and local pools.

Favorites reference endpoints and stay mode-aware. Monthly popularity or
favorite evolution must rebuild or version the affected weights; the
production favorite-evolution pass is not implemented.

## Delivery order and acceptance

1. Provenance (byte-neutral): manifest, normalized schemas, input
   validation, ZIP/ZCTA match reporting, no model reads.
2. World (model-moving): organization/outlet/endpoint synthesis from
   aggregate cells; category and geography coverage gates; named re-pin.
3. Transaction mode (model-moving): persist in-person/remote mode; make
   selection and `use_chip` follow from it.
4. Fraud (model-moving): compromised instrument, modality, endpoint,
   device/IP and geography form one coherent event.

Gates: deterministic replay; a documented fallback for every occupied
area; supply by category and geography matching the cells within scaling
tolerance; no online endpoint with outlet coordinates; no in-person
transaction at a remote-only endpoint; bounded memory at the 20k/29-year
target.
