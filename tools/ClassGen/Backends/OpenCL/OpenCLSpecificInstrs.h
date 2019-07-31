/**
 * Copyright (c) 2017-present, Facebook, Inc.
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

#ifdef GLOW_WITH_OPENCL

BB.newBackendSpecificInstr("OCLBatchedReduceAdd")
    .addOperand("Dest", OperandKind::Out)
    .addOperand("Src", OperandKind::In)
    .addOperand("DestSliceSizes", OperandKind::In)
    .addOperand("SrcSliceSizes", OperandKind::In)
    .addMember(MemberType::Unsigned, "Axis")
    .addMember(MemberType::Unsigned, "AxisSrcSliceSize")
    .autoVerify(VerifyKind::SameElementType, {"Dest", "Src"})
    .autoIRGen();

BB.includeBackendSpecificVerification(
    "glow/OpenCLSpecificInstrsVerification.h");

#endif // GLOW_WITH_CPU
