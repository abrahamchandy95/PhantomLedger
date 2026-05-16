#pragma once

#include "phantomledger/exporter/schema.hpp"

#include <array>
#include <string_view>

namespace PhantomLedger::exporter::schema::aml_txn_edges {

using ::PhantomLedger::exporter::schema::Table;
using ::PhantomLedger::exporter::schema::detail::make;

inline constexpr std::array<std::string_view, 7> kCustomerHeader{
    "id",     "customer_id", "customer_type", "risk_score",
    "status", "kyc_date",    "country",
};
inline constexpr Table kCustomer = make("Customer.csv", kCustomerHeader);

inline constexpr std::array<std::string_view, 21> kAccountHeader{
    "id",
    "account_id",
    "account_type",
    "status",
    "open_date",
    "branch",
    "balance",
    "pagerank_score",
    "louvain_community_id",
    "wcc_component_id",
    "component_size",
    "shortest_path_to_mule",
    "ip_collision_count",
    "device_collision_count",
    "trans_in_mule_ratio",
    "trans_out_mule_ratio",
    "multi_hop_mule_count",
    "betweenness",
    "in_degree",
    "out_degree",
    "clustering_coeff",
};
inline constexpr Table kAccount = make("Account.csv", kAccountHeader);

inline constexpr std::array<std::string_view, 5> kCounterpartyHeader{
    "id", "counterparty_id", "name", "country", "bank_name",
};
inline constexpr Table kCounterparty =
    make("Counterparty.csv", kCounterpartyHeader);

inline constexpr std::array<std::string_view, 5> kBankHeader{
    "id", "bank_id", "name", "swift_code", "country",
};
inline constexpr Table kBank = make("Bank.csv", kBankHeader);

inline constexpr std::array<std::string_view, 6> kDeviceHeader{
    "id", "device_id", "device_type", "os", "first_seen", "last_seen",
};
inline constexpr Table kDevice = make("Device.csv", kDeviceHeader);

inline constexpr std::array<std::string_view, 5> kIpHeader{
    "id", "ip_address", "geo_country", "geo_city", "is_vpn",
};
inline constexpr Table kIp = make("IP.csv", kIpHeader);

inline constexpr std::array<std::string_view, 2> kFullNameHeader{
    "id",
    "name_text",
};
inline constexpr Table kFullName = make("FullName.csv", kFullNameHeader);

inline constexpr std::array<std::string_view, 2> kEmailHeader{
    "id",
    "email_address",
};
inline constexpr Table kEmail = make("Email.csv", kEmailHeader);

inline constexpr std::array<std::string_view, 2> kPhoneHeader{
    "id",
    "phone_number",
};
inline constexpr Table kPhone = make("Phone.csv", kPhoneHeader);

inline constexpr std::array<std::string_view, 2> kDobHeader{
    "id",
    "date_of_birth",
};
inline constexpr Table kDob = make("DOB.csv", kDobHeader);

inline constexpr std::array<std::string_view, 3> kGovtIdHeader{
    "id",
    "id_type",
    "id_value_hash",
};
inline constexpr Table kGovtId = make("GovtID.csv", kGovtIdHeader);

inline constexpr std::array<std::string_view, 6> kAddressHeader{
    "id", "street", "city", "state", "zip", "country",
};
inline constexpr Table kAddress = make("Address.csv", kAddressHeader);

inline constexpr std::array<std::string_view, 5> kWatchlistHeader{
    "id", "list_name", "list_type", "match_score", "effective_date",
};
inline constexpr Table kWatchlist = make("Watchlist.csv", kWatchlistHeader);

inline constexpr std::array<std::string_view, 6> kAlertHeader{
    "id", "alert_id", "rule_id", "created_date", "status", "priority",
};
inline constexpr Table kAlert = make("Alert.csv", kAlertHeader);

inline constexpr std::array<std::string_view, 7> kDispositionHeader{
    "id",           "disposition_id", "action",     "close_code",
    "investigator", "date",           "notes_hash",
};
inline constexpr Table kDisposition =
    make("Disposition.csv", kDispositionHeader);

inline constexpr std::array<std::string_view, 5> kSarHeader{
    "id", "sar_id", "filing_date", "typology", "status",
};
inline constexpr Table kSar = make("SAR.csv", kSarHeader);

inline constexpr std::array<std::string_view, 6> kCtrHeader{
    "id", "ctr_id", "filing_date", "amount", "branch", "teller_id",
};
inline constexpr Table kCtr = make("CTR.csv", kCtrHeader);

inline constexpr std::array<std::string_view, 3> kMinHashBucketHeader{
    "id",
    "bucket_id",
    "hash_band",
};
inline constexpr Table kMinHashBucket =
    make("MinHashBucket.csv", kMinHashBucketHeader);

inline constexpr std::array<std::string_view, 7> kInvestigationCaseHeader{
    "id",          "case_id",     "case_type",   "status",
    "opened_date", "closed_date", "case_system",
};
inline constexpr Table kInvestigationCase =
    make("InvestigationCase.csv", kInvestigationCaseHeader);

inline constexpr std::array<std::string_view, 7> kEvidenceArtifactHeader{
    "id",        "artifact_id", "artifact_type", "content_hash",
    "store_uri", "created_at",  "source_system",
};
inline constexpr Table kEvidenceArtifact =
    make("EvidenceArtifact.csv", kEvidenceArtifactHeader);

inline constexpr std::array<std::string_view, 6> kBusinessHeader{
    "id",          "business_id", "legal_name",
    "entity_type", "country",     "registration_id",
};
inline constexpr Table kBusiness = make("Business.csv", kBusinessHeader);

inline constexpr std::array<std::string_view, 11> kChainHeader{
    "id",       "chain_id",  "ring_id",          "typology",
    "num_hops", "principal", "final_amount",     "total_haircut",
    "start_ts", "end_ts",    "duration_seconds",
};
inline constexpr Table kChain = make("Chain.csv", kChainHeader);

inline constexpr std::array<std::string_view, 13> kInvestigationCaseTxnHeader{
    "id",
    "transaction_id",
    "case_id",
    "source_account_id",
    "target_account_id",
    "timestamp",
    "amount",
    "currency_code",
    "channel_code",
    "beneficiary_country_code",
    "credit_debit",
    "promoted_at",
    "ttl_date",
};
inline constexpr Table kInvestigationCaseTxn =
    make("InvestigationCaseTxn.csv", kInvestigationCaseTxnHeader);

inline constexpr std::array<std::string_view, 4> kConnectedComponentHeader{
    "id",
    "component_id",
    "size",
    "computed_date",
};
inline constexpr Table kConnectedComponent =
    make("ConnectedComponent.csv", kConnectedComponentHeader);

inline constexpr std::array<std::string_view, 6> kOwnsHeader{
    "from_id",       "to_id",      "effective_date",
    "source_system", "valid_from", "valid_to",
};
inline constexpr Table kOwns = make("OWNS.csv", kOwnsHeader);

inline constexpr std::array<std::string_view, 10> kTransactedHeader{
    "from_id",        "to_id",
    "transaction_id", "timestamp",
    "amount",         "currency_code",
    "channel_code",   "beneficiary_country_code",
    "credit_debit",   "source_system",
};
inline constexpr Table kTransacted = make("TRANSACTED.csv", kTransactedHeader);

inline constexpr std::array<std::string_view, 3> kTransactionChainLabelHeader{
    "transaction_id",
    "chain_id",
    "ring_id",
};
inline constexpr Table kTransactionChainLabel =
    make("TRANSACTION_CHAIN_LABEL.csv", kTransactionChainLabelHeader);

inline constexpr std::array<std::string_view, 6> kInvolvesCounterpartyHeader{
    "from_id",       "to_id",      "observed_at",
    "source_system", "valid_from", "valid_to",
};
inline constexpr Table kInvolvesCounterparty =
    make("INVOLVES_COUNTERPARTY.csv", kInvolvesCounterpartyHeader);

inline constexpr std::array<std::string_view, 5> kBanksAtHeader{
    "from_id", "to_id", "source_system", "valid_from", "valid_to",
};
inline constexpr Table kBanksAt = make("BANKS_AT.csv", kBanksAtHeader);

inline constexpr std::array<std::string_view, 8> kOnWatchlistHeader{
    "from_id",     "to_id",        "match_date", "source_system",
    "match_score", "evidence_ref", "valid_from", "valid_to",
};
inline constexpr Table kOnWatchlist =
    make("ON_WATCHLIST.csv", kOnWatchlistHeader);

inline constexpr std::array<std::string_view, 5> kSubjectOfSarHeader{
    "from_id", "to_id", "filing_date", "source_system", "evidence_ref",
};
inline constexpr Table kSubjectOfSar =
    make("SUBJECT_OF_SAR.csv", kSubjectOfSarHeader);

inline constexpr std::array<std::string_view, 4> kFiledCtrHeader{
    "from_id",
    "to_id",
    "filing_date",
    "source_system",
};
inline constexpr Table kFiledCtr = make("FILED_CTR.csv", kFiledCtrHeader);

inline constexpr std::array<std::string_view, 5> kAlertOnHeader{
    "from_id", "to_id", "alert_date", "rule_id", "source_system",
};
inline constexpr Table kAlertOn = make("ALERT_ON.csv", kAlertOnHeader);

inline constexpr std::array<std::string_view, 8> kDispositionedAsHeader{
    "from_id",        "to_id",         "decision_date", "investigator_id",
    "rationale_hash", "evidence_refs", "source_system", "confidence",
};
inline constexpr Table kDispositionedAs =
    make("DISPOSITIONED_AS.csv", kDispositionedAsHeader);

inline constexpr std::array<std::string_view, 5> kEscalatedToHeader{
    "from_id", "to_id", "escalation_date", "source_system", "evidence_refs",
};
inline constexpr Table kEscalatedTo =
    make("ESCALATED_TO.csv", kEscalatedToHeader);

inline constexpr std::array<std::string_view, 4> kContainsAlertHeader{
    "from_id",
    "to_id",
    "created_at",
    "source_system",
};
inline constexpr Table kContainsAlert =
    make("CONTAINS_ALERT.csv", kContainsAlertHeader);

inline constexpr std::array<std::string_view, 4> kResultedInHeader{
    "from_id",
    "to_id",
    "created_at",
    "source_system",
};
inline constexpr Table kResultedIn = make("RESULTED_IN.csv", kResultedInHeader);

inline constexpr std::array<std::string_view, 4> kHasEvidenceHeader{
    "from_id",
    "to_id",
    "created_at",
    "source_system",
};
inline constexpr Table kHasEvidence =
    make("HAS_EVIDENCE.csv", kHasEvidenceHeader);

inline constexpr std::array<std::string_view, 4> kContainsPromotedTxnHeader{
    "from_id",
    "to_id",
    "created_at",
    "source_system",
};
inline constexpr Table kContainsPromotedTxn =
    make("CONTAINS_PROMOTED_TXN.csv", kContainsPromotedTxnHeader);

inline constexpr std::array<std::string_view, 5> kPromotedTxnAccountHeader{
    "from_id", "to_id", "role", "created_at", "source_system",
};
inline constexpr Table kPromotedTxnAccount =
    make("PROMOTED_TXN_ACCOUNT.csv", kPromotedTxnAccountHeader);

inline constexpr std::array<std::string_view, 5> kSignerOfHeader{
    "from_id", "to_id", "effective_date", "valid_from", "valid_to",
};
inline constexpr Table kSignerOf = make("SIGNER_OF.csv", kSignerOfHeader);

inline constexpr std::array<std::string_view, 6> kBeneficialOwnerOfHeader{
    "from_id",        "to_id",      "ownership_pct",
    "effective_date", "valid_from", "valid_to",
};
inline constexpr Table kBeneficialOwnerOf =
    make("BENEFICIAL_OWNER_OF.csv", kBeneficialOwnerOfHeader);

inline constexpr std::array<std::string_view, 6> kControlsHeader{
    "from_id",        "to_id",      "control_type",
    "effective_date", "valid_from", "valid_to",
};
inline constexpr Table kControls = make("CONTROLS.csv", kControlsHeader);

inline constexpr std::array<std::string_view, 5> kBusinessOwnsAccountHeader{
    "from_id", "to_id", "effective_date", "valid_from", "valid_to",
};
inline constexpr Table kBusinessOwnsAccount =
    make("BUSINESS_OWNS_ACCOUNT.csv", kBusinessOwnsAccountHeader);

inline constexpr std::array<std::string_view, 4> kHasNameHeader{
    "from_id",
    "to_id",
    "observed_at",
    "source_system",
};
inline constexpr Table kHasName = make("HAS_NAME.csv", kHasNameHeader);

inline constexpr std::array<std::string_view, 4> kHasAddressHeader{
    "from_id",
    "to_id",
    "observed_at",
    "source_system",
};
inline constexpr Table kHasAddress = make("HAS_ADDRESS.csv", kHasAddressHeader);

inline constexpr std::array<std::string_view, 4> kHasEmailHeader{
    "from_id",
    "to_id",
    "observed_at",
    "source_system",
};
inline constexpr Table kHasEmail = make("HAS_EMAIL.csv", kHasEmailHeader);

inline constexpr std::array<std::string_view, 4> kHasPhoneHeader{
    "from_id",
    "to_id",
    "observed_at",
    "source_system",
};
inline constexpr Table kHasPhone = make("HAS_PHONE.csv", kHasPhoneHeader);

inline constexpr std::array<std::string_view, 3> kHasDobHeader{
    "from_id",
    "to_id",
    "source_system",
};
inline constexpr Table kHasDob = make("HAS_DOB.csv", kHasDobHeader);

inline constexpr std::array<std::string_view, 4> kHasIdHeader{
    "from_id",
    "to_id",
    "source_system",
    "confidence",
};
inline constexpr Table kHasId = make("HAS_ID.csv", kHasIdHeader);

inline constexpr std::array<std::string_view, 6> kUsesDeviceHeader{
    "from_id",   "to_id",         "first_seen",
    "last_seen", "source_system", "confidence",
};
inline constexpr Table kUsesDevice = make("USES_DEVICE.csv", kUsesDeviceHeader);

inline constexpr std::array<std::string_view, 6> kUsesIpHeader{
    "from_id",   "to_id",         "first_seen",
    "last_seen", "source_system", "confidence",
};
inline constexpr Table kUsesIp = make("USES_IP.csv", kUsesIpHeader);

inline constexpr std::array<std::string_view, 4> kInBucketHeader{
    "from_id",
    "to_id",
    "computed_date",
    "confidence",
};
inline constexpr Table kInBucket = make("IN_BUCKET.csv", kInBucketHeader);

inline constexpr std::array<std::string_view, 14> kAccountFlowAggHeader{
    "from_id",        "to_id",
    "total_amount",   "txn_count",
    "first_txn_date", "last_txn_date",
    "amount_30d",     "amount_90d",
    "count_30d",      "count_90d",
    "window_start",   "window_end",
    "computed_date",  "derivation_run_id",
};
inline constexpr Table kAccountFlowAgg =
    make("ACCOUNT_FLOW_AGG.csv", kAccountFlowAggHeader);

inline constexpr std::array<std::string_view, 15> kAccountLinkCommHeader{
    "from_id",       "to_id",         "weight",
    "txn_count",     "total_amount",  "first_txn_date",
    "last_txn_date", "amount_30d",    "amount_90d",
    "count_30d",     "count_90d",     "window_start",
    "window_end",    "computed_date", "derivation_run_id",
};
inline constexpr Table kAccountLinkComm =
    make("ACCOUNT_LINK_COMM.csv", kAccountLinkCommHeader);

inline constexpr std::array<std::string_view, 4> kInClusterHeader{
    "from_id",
    "to_id",
    "computed_date",
    "algorithm",
};
inline constexpr Table kInCluster = make("IN_CLUSTER.csv", kInClusterHeader);

inline constexpr std::array<std::string_view, 7> kSameAsHeader{
    "from_id",       "to_id",     "weight",     "match_fields",
    "computed_date", "algorithm", "confidence",
};
inline constexpr Table kSameAs = make("SAME_AS.csv", kSameAsHeader);

} // namespace PhantomLedger::exporter::schema::aml_txn_edges
