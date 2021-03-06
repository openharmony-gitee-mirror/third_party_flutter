// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These packages exist in the third_party/dart .package, but not locally
// which confuses the analyzer.
// ignore_for_file: uri_does_not_exist
import 'dart:async';
import 'dart:convert' show json;
import 'dart:io';
import 'package:args/args.dart' show ArgParser;
import 'package:dev_compiler/src/compiler/module_builder.dart';
import 'package:dev_compiler/src/compiler/shared_command.dart'
    show SharedCompilerOptions;
import 'package:dev_compiler/src/kernel/target.dart';
import 'package:dev_compiler/src/kernel/command.dart';
import 'package:dev_compiler/src/kernel/compiler.dart';
import 'package:front_end/src/api_unstable/ddc.dart'
    show
        CompilerOptions,
        kernelForModule,
        DiagnosticMessage,
        printDiagnosticMessage,
        Severity;
import 'package:kernel/kernel.dart';
import 'package:kernel/target/targets.dart';
import 'package:path/path.dart' as path;

// This script is forked from https://github.com/dart-lang/sdk/blob/master/pkg/dev_compiler/tool/kernel_sdk.dart
// and produces the precompiled sdk for dartdevc. This has been modified to include a dart:ui target.
Future main(List<String> args) async {
  var ddcPath = path.dirname(path.dirname(path.fromUri(Platform.script)));

  // Parse flags.
  var parser = ArgParser()
    ..addOption('output')
    ..addOption('libraries',
        defaultsTo: path.join(ddcPath, '../../sdk/lib/libraries.json'));
  var parserOptions = parser.parse(args);
  var outputPath = parserOptions['output'] as String;
  if (outputPath == null) {
    var sdkRoot = path.absolute(path.dirname(path.dirname(ddcPath)));
    var buildDir = path.join(sdkRoot, Platform.isMacOS ? 'xcodebuild' : 'out');
    var genDir = path.join(buildDir, 'ReleaseX64', 'gen', 'utils', 'dartdevc');
    outputPath = path.join(genDir, 'kernel', 'ddc_sdk.dill');
  }

  var librarySpecPath = parserOptions['libraries'] as String;
  var packagesPath = path.join(ddcPath, '../third_party/dart/.packages');
  void onDiagnostic(DiagnosticMessage message) {
    printDiagnosticMessage(message, print);
    if (message.severity == Severity.error ||
        message.severity == Severity.internalProblem) {
      exitCode = 1;
    }
  }

  var target = FlutterDevCompilerTarget();
  var options = CompilerOptions()
    ..compileSdk = true
    // TODO(sigmund): remove this unnecessary option when possible.
    ..sdkRoot = Uri.base
    ..packagesFileUri = Uri.base.resolveUri(Uri.file(packagesPath))
    ..librariesSpecificationUri = Uri.base.resolveUri(Uri.file(librarySpecPath))
    ..target = target
    ..onDiagnostic = onDiagnostic
    ..environmentDefines = {};

  var inputs = target.extraRequiredLibraries.map(Uri.parse).toList();
  var compilerResult = await kernelForModule(inputs, options);
  var component = compilerResult.component;

  var outputDir = path.dirname(outputPath);
  await Directory(outputDir).create(recursive: true);
  await writeComponentToBinary(component, outputPath);

  var jsModule = ProgramCompiler(
      component,
      compilerResult.classHierarchy,
      SharedCompilerOptions(moduleName: 'dart_sdk'),
      {}).emitModule(component, [], [], {});
  var moduleFormats = {
    'amd': ModuleFormat.amd,
  };

  for (var name in moduleFormats.keys) {
    var format = moduleFormats[name];
    var jsDir = path.join(outputDir, name);
    var jsPath = path.join(jsDir, 'dart_sdk.js');
    await Directory(jsDir).create();
    var jsCode = jsProgramToCode(jsModule, format);
    await File(jsPath).writeAsString(jsCode.code);
    await File('$jsPath.map').writeAsString(json.encode(jsCode.sourceMap));
  }
}

class FlutterDevCompilerTarget extends DevCompilerTarget {
  FlutterDevCompilerTarget() : super(TargetFlags());

  @override
  List<String> get extraRequiredLibraries => const [
        'dart:_runtime',
        'dart:_debugger',
        'dart:_foreign_helper',
        'dart:_interceptors',
        'dart:_internal',
        'dart:_isolate_helper',
        'dart:_js_helper',
        'dart:_js_mirrors',
        'dart:_js_primitives',
        'dart:_metadata',
        'dart:_native_typed_data',
        'dart:async',
        'dart:collection',
        'dart:convert',
        'dart:developer',
        'dart:io',
        'dart:isolate',
        'dart:js',
        'dart:js_util',
        'dart:math',
        'dart:mirrors',
        'dart:typed_data',
        'dart:indexed_db',
        'dart:html',
        'dart:html_common',
        'dart:svg',
        'dart:web_audio',
        'dart:web_gl',
        'dart:web_sql',
        'dart:ui',
        'dart:_engine',
      ];
}
