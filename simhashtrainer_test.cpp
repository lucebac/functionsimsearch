#include "gtest/gtest.h"
#include "functionsimhash.hpp"
#include "simhashtrainer.hpp"
#include "util.hpp"
#include <array>

// This test calculates the similarity between RarVM::ExecuteStandardFilter
// in various optimization settings, both pre- and post-training.
std::map<uint64_t, const std::string> id_to_filename = {
  { 0xf89b73cc72cd02c7ULL, "../testdata/unrar.5.5.3.builds/unrar.x64.O0" },
  { 0x5ae018cfafb410f5ULL, "../testdata/unrar.5.5.3.builds/unrar.x64.O2" },
  { 0x51f3962ff93c1c1eULL, "../testdata/unrar.5.5.3.builds/unrar.x64.O3" },
  { 0x584e2f1630b21cfaULL, "../testdata/unrar.5.5.3.builds/unrar.x64.Os" },
  { 0xf7f94f1cdfbe0f98ULL, "../testdata/unrar.5.5.3.builds/unrar.x86.O0" },
  { 0x83fe3244c90314f4ULL, "../testdata/unrar.5.5.3.builds/unrar.x86.O2" },
  { 0x396063026eaac371ULL, "../testdata/unrar.5.5.3.builds/unrar.x86.O3" },
  { 0x924daa0b17c6ae64ULL, "../testdata/unrar.5.5.3.builds/unrar.x86.Os" }};

std::map<uint64_t, const std::string> id_to_mode = {
  { 0xf89b73cc72cd02c7ULL, "ELF" },
  { 0x5ae018cfafb410f5ULL, "ELF" },
  { 0x51f3962ff93c1c1eULL, "ELF" },
  { 0x584e2f1630b21cfaULL, "ELF" },
  { 0xf7f94f1cdfbe0f98ULL, "ELF" },
  { 0x83fe3244c90314f4ULL, "ELF" },
  { 0x396063026eaac371ULL, "ELF" },
  { 0x924daa0b17c6ae64ULL, "ELF" }};

// The addresses of RarVM::ExecuteStandardFilter in the various executables
// listed above.
std::map<uint64_t, uint64_t> id_to_address_function_1 = {
  { 0xf89b73cc72cd02c7, 0x0000000000418400 },
  { 0x5ae018cfafb410f5, 0x0000000000411890 },
  { 0x51f3962ff93c1c1e, 0x0000000000416f60 },
  { 0x584e2f1630b21cfa, 0x000000000040e092 },
  { 0xf7f94f1cdfbe0f98, 0x000000000805e09c },
  { 0x83fe3244c90314f4, 0x0000000008059f70 },
  { 0x396063026eaac371, 0x000000000805e910 },
  { 0x924daa0b17c6ae64, 0x00000000080566fc } };


TEST(simhashtrainer, simple_attraction) {
  std::vector<FunctionFeatures> all_functions;
  std::vector<FeatureHash> all_features_vector;
  std::vector<std::pair<uint32_t, uint32_t>> attractionset;
  std::vector<std::pair<uint32_t, uint32_t>> repulsionset;

  ASSERT_TRUE(LoadTrainingData("../testdata/train_simple_attraction",
    &all_functions, &all_features_vector, &attractionset, &repulsionset));

  SimHashTrainer trainer(
    &all_functions,
    &all_features_vector,
    &attractionset,
    &repulsionset);
  std::vector<double> weights;
  trainer.Train(&weights);

  // The total feature set contains 4 elements.
  ASSERT_EQ(all_features_vector.size(), 4);

  // We expect that the weight of the first two features (which are not shared)
  // should be close to zero, and the weight of the last two features (which
  // are shared) should be close to one.

  EXPECT_TRUE(weights[0] < 0.01);
  EXPECT_TRUE(weights[1] < 0.01);
  EXPECT_TRUE(weights[2] > 0.90);
  EXPECT_TRUE(weights[3] > 0.90);

  std::map<uint64_t, float> hash_to_weight;

  for (uint32_t index = 0; index < all_features_vector.size(); ++index) {
    printf("Feature %16lx%16lx weight %f\n", all_features_vector[index].first,
      all_features_vector[index].second, weights[index]);
    hash_to_weight[all_features_vector[index].first] = weights[index];
  }
 
  // Instantiate two simhashers - one without the trained weights and one with
  // the trained weights - to ensure that the hamming distance between the
  // hashes is reduced by the training procedure.
  FunctionSimHasher hasher_untrained("");
  FunctionSimHasher hasher_trained(&hash_to_weight);

  uint64_t fileA = 0x396063026eaac371ULL;
  uint64_t addressA = 0x000000000805e910ULL;
  uint64_t fileB = 0x51f3962ff93c1c1eULL;
  uint64_t addressB = 0x0000000000416f60ULL;

  FeatureHash untrainedA = GetHashForFileAndFunction(hasher_untrained,
    id_to_filename[fileA], id_to_mode[fileA], addressA);
  FeatureHash trainedA = GetHashForFileAndFunction(hasher_trained,
      id_to_filename[fileA], id_to_mode[fileA], addressA);

  FeatureHash untrainedB = GetHashForFileAndFunction(hasher_untrained,
    id_to_filename[fileB], id_to_mode[fileB], addressB);
  FeatureHash trainedB = GetHashForFileAndFunction(hasher_trained,
      id_to_filename[fileB], id_to_mode[fileB], addressB);

  printf("Hash of A is %16.16lx%16.16lx untrained\n", untrainedA.first,
    untrainedA.second);

  printf("Hash of B is %16.16lx%16.16lx untrained\n", untrainedB.first,
    untrainedB.second);

  printf("Hamming distance is %d\n", HammingDistance(untrainedA, untrainedB));

  printf("Hash of A is %16.16lx%16.16lx trained\n", trainedA.first,
    trainedA.second);

  printf("Hash of B is %16.16lx%16.16lx trained\n", trainedB.first,
    trainedB.second);

  printf("Hamming distance is %d\n", HammingDistance(trainedA, trainedB));


}


// A test to validate that two functions from an attractionset will indeed have
// their distance reduced by the training procedure.
TEST(simhashtrainer, attractionset) {
  // Initialize an untrained hasher.
  FunctionSimHasher hasher_untrained("");

  // Train weights on the testing data, and save them to the temp directory.
  ASSERT_EQ(TrainSimHashFromDataDirectory("../testdata/train_attraction_only",
    "/tmp/attraction_weights.txt"), true);

  // Initialize a trained hasher.
  FunctionSimHasher hasher_trained("/tmp/attraction_weights.txt");

  printf("[!] Ran training\n");
  // Get the hashes for all functions above, with both hashers.
  std::map<std::pair<uint64_t, uint64_t>, FeatureHash> hashes_untrained;
  std::map<std::pair<uint64_t, uint64_t>, FeatureHash> hashes_trained;

  for (const auto& hash_addr : id_to_address_function_1) {
    uint64_t file_hash = hash_addr.first;
    uint64_t address = hash_addr.second;

    FeatureHash untrained = GetHashForFileAndFunction(hasher_untrained,
      id_to_filename[file_hash], id_to_mode[file_hash], address);
    FeatureHash trained = GetHashForFileAndFunction(hasher_trained,
      id_to_filename[file_hash], id_to_mode[file_hash], address);

    ASSERT_TRUE((untrained.first != 0) || (untrained.second !=0));
    ASSERT_TRUE((trained.first != 0) || (trained.second !=0));

    hashes_untrained[hash_addr] = untrained;
    hashes_trained[hash_addr] = trained;
  }

  printf("[!] Calculated the hashes!\n");

  // All the untrained and trained hashes are available now.
  // Calculate a similarity matrix using both and dump it out.
  printf("[!] Untrained hamming distances:\n");
  for (const auto& hash_addr_A : hashes_untrained) {
    for (const auto& hash_addr_B : hashes_untrained) {
      FeatureHash hash_A = hash_addr_A.second;
      FeatureHash hash_B = hash_addr_B.second;

      printf("%d ", HammingDistance(hash_A, hash_B));
    }
    printf("\n");
  }
  printf("[!] Trained hamming distances:\n");
  for (const auto& hash_addr_A : hashes_trained) {
    for (const auto& hash_addr_B : hashes_trained) {
      FeatureHash hash_A = hash_addr_A.second;
      FeatureHash hash_B = hash_addr_B.second;

      printf("%d ", HammingDistance(hash_A, hash_B));
    }
    printf("\n");
  }

}

