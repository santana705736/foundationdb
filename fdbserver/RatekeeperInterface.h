/*
 * RatekeeperInterface.h
 *
 * This source file is part of the FoundationDB open source project
 *
 * Copyright 2013-2019 Apple Inc. and the FoundationDB project authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FDBSERVER_RATEKEEPERINTERFACE_H
#define FDBSERVER_RATEKEEPERINTERFACE_H

#include "fdbclient/CommitProxyInterface.h"
#include "fdbclient/FDBTypes.h"
#include "fdbrpc/fdbrpc.h"
#include "fdbrpc/Locality.h"

struct RatekeeperInterface {
	constexpr static FileIdentifier file_identifier = 5983305;
	RequestStream<ReplyPromise<Void>> waitFailure;
	RequestStream<struct GetRateInfoRequest> getRateInfo;
	RequestStream<struct HaltRatekeeperRequest> haltRatekeeper;
	RequestStream<struct ReportCommitCostEstimationRequest> reportCommitCostEstimation;
	// TODO REMOVE!!!
	RequestStream<struct BlobGranuleFileRequest> blobGranuleFileRequest;
	struct LocalityData locality;
	UID myId;

	RatekeeperInterface() {}
	explicit RatekeeperInterface(const struct LocalityData& l, UID id) : locality(l), myId(id) {}

	void initEndpoints() {}
	UID id() const { return myId; }
	NetworkAddress address() const { return getRateInfo.getEndpoint().getPrimaryAddress(); }
	bool operator==(const RatekeeperInterface& r) const { return id() == r.id(); }
	bool operator!=(const RatekeeperInterface& r) const { return !(*this == r); }

	template <class Archive>
	void serialize(Archive& ar) {
		serializer(ar,
		           waitFailure,
		           getRateInfo,
		           haltRatekeeper,
		           reportCommitCostEstimation,
		           blobGranuleFileRequest,
		           locality,
		           myId);
	}
};

struct TransactionCommitCostEstimation {
	int opsSum = 0;
	uint64_t costSum = 0;

	uint64_t getCostSum() const { return costSum; }
	int getOpsSum() const { return opsSum; }

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, opsSum, costSum);
	}

	TransactionCommitCostEstimation& operator+=(const TransactionCommitCostEstimation& other) {
		opsSum += other.opsSum;
		costSum += other.costSum;
		return *this;
	}
};

struct GetRateInfoReply {
	constexpr static FileIdentifier file_identifier = 7845006;
	double transactionRate;
	double batchTransactionRate;
	double leaseDuration;
	HealthMetrics healthMetrics;

	Optional<PrioritizedTransactionTagMap<ClientTagThrottleLimits>> throttledTags;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, transactionRate, batchTransactionRate, leaseDuration, healthMetrics, throttledTags);
	}
};

struct GetRateInfoRequest {
	constexpr static FileIdentifier file_identifier = 9068521;
	UID requesterID;
	int64_t totalReleasedTransactions;
	int64_t batchReleasedTransactions;

	TransactionTagMap<uint64_t> throttledTagCounts;
	bool detailed;
	ReplyPromise<struct GetRateInfoReply> reply;

	GetRateInfoRequest() {}
	GetRateInfoRequest(UID const& requesterID,
	                   int64_t totalReleasedTransactions,
	                   int64_t batchReleasedTransactions,
	                   TransactionTagMap<uint64_t> throttledTagCounts,
	                   bool detailed)
	  : requesterID(requesterID), totalReleasedTransactions(totalReleasedTransactions),
	    batchReleasedTransactions(batchReleasedTransactions), throttledTagCounts(throttledTagCounts),
	    detailed(detailed) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(
		    ar, requesterID, totalReleasedTransactions, batchReleasedTransactions, throttledTagCounts, detailed, reply);
	}
};

struct HaltRatekeeperRequest {
	constexpr static FileIdentifier file_identifier = 6997218;
	UID requesterID;
	ReplyPromise<Void> reply;

	HaltRatekeeperRequest() {}
	explicit HaltRatekeeperRequest(UID uid) : requesterID(uid) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, requesterID, reply);
	}
};

struct ReportCommitCostEstimationRequest {
	constexpr static FileIdentifier file_identifier = 8314904;
	UIDTransactionTagMap<TransactionCommitCostEstimation> ssTrTagCommitCost;
	ReplyPromise<Void> reply;

	ReportCommitCostEstimationRequest() {}
	ReportCommitCostEstimationRequest(UIDTransactionTagMap<TransactionCommitCostEstimation> ssTrTagCommitCost)
	  : ssTrTagCommitCost(ssTrTagCommitCost) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, ssTrTagCommitCost, reply);
	}
};

// TODO MOVE ELSEWHERE
struct MutationAndVersion {
	constexpr static FileIdentifier file_identifier = 4268041;
	MutationRef m;
	Version v;

	MutationAndVersion() {}
	MutationAndVersion(Arena& to, MutationRef m, Version v) : m(to, m), v(v) {}
	MutationAndVersion(Arena& to, const MutationAndVersion& from) : m(to, from.m), v(from.v) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, m, v);
	}
};

// the assumption of this response is
struct BlobGranuleChunk {
	KeyRangeRef keyRange;
	StringRef snapshotFileName;
	VectorRef<StringRef> deltaFileNames;
	VectorRef<MutationAndVersion> newDeltas;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, keyRange, snapshotFileName, deltaFileNames, newDeltas);
	}
};

struct BlobGranuleFileReply {
	// TODO "proper" way to generate file_identifier?
	constexpr static FileIdentifier file_identifier = 6858612;
	Arena arena;
	VectorRef<BlobGranuleChunk> chunks;

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, chunks, arena);
	}
};

// TODO could do a reply promise stream of file mutations to bound memory requirements?
// Have to load whole snapshot file into memory though so it doesn't actually matter too much
struct BlobGranuleFileRequest {

	constexpr static FileIdentifier file_identifier = 4150141;
	Arena arena;
	KeyRangeRef keyRange;
	Version readVersion;
	ReplyPromise<BlobGranuleFileReply> reply;

	BlobGranuleFileRequest() {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, keyRange, readVersion, reply, arena);
	}
};

#endif // FDBSERVER_RATEKEEPERINTERFACE_H
