#!/usr/bin/env python3
"""
LabJack U12 Test Log Analyzer
Processes test suite logs to identify issues and generate development insights
"""

import os
import sys
import re
import json
from pathlib import Path
from collections import defaultdict, Counter

class TestLogAnalyzer:
    def __init__(self, log_dir):
        self.log_dir = Path(log_dir)
        self.results = {}
        self.issues = []
        self.insights = []
        
    def analyze_all_logs(self):
        """Analyze all log files in the directory"""
        print(f"Analyzing logs in: {self.log_dir}")
        
        # System information
        self.analyze_system_info()
        
        # Test results
        self.analyze_test_results()
        
        # Error patterns
        self.analyze_error_patterns()
        
        # Performance metrics
        self.analyze_performance()
        
        # Generate recommendations
        self.generate_recommendations()
        
        return self.create_report()
    
    def analyze_system_info(self):
        """Extract system and hardware information"""
        system_log = self.log_dir / "system_info.log"
        if not system_log.exists():
            return
            
        with open(system_log, 'r') as f:
            content = f.read()
        
        # Extract key information
        self.results['system'] = {
            'kernel': self.extract_pattern(content, r'Kernel: (.+)'),
            'distribution': self.extract_pattern(content, r'Distribution: (.+)'),
            'usb_devices': self.count_pattern(content, r'Bus \d+ Device \d+'),
            'labjack_found': 'LabJack' in content,
            'hid_devices': self.count_pattern(content, r'/dev/hidraw\d+'),
            'dmesg_entries': self.count_pattern(content, r'labjack|0cd5|0001')
        }
    
    def analyze_test_results(self):
        """Analyze individual test results"""
        test_results = {}
        
        for log_file in self.log_dir.glob("*.log"):
            if log_file.name in ['system_info.log', 'libsigrok_info.log', 'test_summary.log']:
                continue
                
            with open(log_file, 'r') as f:
                content = f.read()
            
            # Extract test result
            result_match = re.search(r'RESULT: (\w+)', content)
            result = result_match.group(1) if result_match else 'UNKNOWN'
            
            test_results[log_file.stem] = {
                'result': result,
                'content': content,
                'errors': self.extract_errors(content),
                'warnings': self.extract_warnings(content)
            }
        
        self.results['tests'] = test_results
    
    def analyze_error_patterns(self):
        """Identify common error patterns across tests"""
        error_patterns = Counter()
        
        for test_name, test_data in self.results.get('tests', {}).items():
            content = test_data['content']
            
            # Common error patterns
            patterns = [
                (r'LIBUSB_ERROR_(\w+)', 'USB Error'),
                (r'Failed to (\w+)', 'Operation Failure'),
                (r'sr: ([^:]+): (.+)', 'LibSigrok Error'),
                (r'segfault', 'Segmentation Fault'),
                (r'timeout', 'Timeout'),
                (r'Permission denied', 'Permission Error'),
                (r'No such device', 'Device Not Found'),
                (r'Interface claim failed', 'USB Interface Error')
            ]
            
            for pattern, category in patterns:
                matches = re.findall(pattern, content, re.IGNORECASE)
                if matches:
                    error_patterns[f"{category}: {matches[0] if isinstance(matches[0], str) else matches[0][0]}"] += 1
        
        self.results['error_patterns'] = dict(error_patterns.most_common(10))
    
    def analyze_performance(self):
        """Extract performance metrics from timing tests"""
        performance = {}
        
        for test_name, test_data in self.results.get('tests', {}).items():
            if 'timing' in test_name:
                content = test_data['content']
                
                # Extract timing information
                real_time = self.extract_pattern(content, r'real\s+(\d+m[\d.]+s)')
                user_time = self.extract_pattern(content, r'user\s+(\d+m[\d.]+s)')
                sys_time = self.extract_pattern(content, r'sys\s+(\d+m[\d.]+s)')
                
                if real_time:
                    performance[test_name] = {
                        'real_time': real_time,
                        'user_time': user_time,
                        'sys_time': sys_time
                    }
        
        self.results['performance'] = performance
    
    def generate_recommendations(self):
        """Generate development recommendations based on analysis"""
        recommendations = []
        
        # Check for critical failures
        failed_tests = [name for name, data in self.results.get('tests', {}).items() 
                       if data['result'] == 'FAIL']
        
        if failed_tests:
            recommendations.append({
                'priority': 'HIGH',
                'category': 'Critical Failures',
                'issue': f"{len(failed_tests)} tests failed unexpectedly",
                'tests': failed_tests,
                'action': 'Review failed test logs for specific error messages'
            })
        
        # Check for USB issues
        usb_errors = [pattern for pattern in self.results.get('error_patterns', {}) 
                     if 'USB' in pattern or 'LIBUSB' in pattern]
        
        if usb_errors:
            recommendations.append({
                'priority': 'HIGH',
                'category': 'USB Communication',
                'issue': 'USB communication errors detected',
                'patterns': usb_errors,
                'action': 'Check USB permissions, HID driver conflicts, and device connectivity'
            })
        
        # Check for permission issues
        if not self.results.get('system', {}).get('labjack_found', False):
            recommendations.append({
                'priority': 'HIGH',
                'category': 'Device Detection',
                'issue': 'LabJack U12 not detected in system',
                'action': 'Verify device is connected and recognized by system'
            })
        
        # Check for HID conflicts
        hid_devices = self.results.get('system', {}).get('hid_devices', 0)
        if hid_devices > 0:
            recommendations.append({
                'priority': 'MEDIUM',
                'category': 'HID Driver Conflict',
                'issue': f'{hid_devices} HID devices detected',
                'action': 'May need to unbind HID driver from LabJack U12'
            })
        
        self.results['recommendations'] = recommendations
    
    def extract_pattern(self, text, pattern):
        """Extract first match of pattern from text"""
        match = re.search(pattern, text)
        return match.group(1) if match else None
    
    def count_pattern(self, text, pattern):
        """Count occurrences of pattern in text"""
        return len(re.findall(pattern, text, re.IGNORECASE))
    
    def extract_errors(self, content):
        """Extract error messages from log content"""
        errors = []
        for line in content.split('\n'):
            if any(keyword in line.lower() for keyword in ['error', 'failed', 'fail']):
                errors.append(line.strip())
        return errors[:5]  # Limit to first 5 errors
    
    def extract_warnings(self, content):
        """Extract warning messages from log content"""
        warnings = []
        for line in content.split('\n'):
            if any(keyword in line.lower() for keyword in ['warning', 'warn']):
                warnings.append(line.strip())
        return warnings[:3]  # Limit to first 3 warnings
    
    def create_report(self):
        """Create comprehensive analysis report"""
        report = {
            'summary': self.create_summary(),
            'system_info': self.results.get('system', {}),
            'test_results': self.create_test_summary(),
            'error_analysis': self.results.get('error_patterns', {}),
            'performance': self.results.get('performance', {}),
            'recommendations': self.results.get('recommendations', []),
            'detailed_failures': self.create_failure_details()
        }
        
        return report
    
    def create_summary(self):
        """Create high-level summary"""
        tests = self.results.get('tests', {})
        total = len(tests)
        passed = sum(1 for t in tests.values() if t['result'] in ['PASS', 'EXPECTED_FAIL'])
        failed = sum(1 for t in tests.values() if t['result'] == 'FAIL')
        
        return {
            'total_tests': total,
            'passed': passed,
            'failed': failed,
            'success_rate': f"{(passed/total*100):.1f}%" if total > 0 else "0%",
            'critical_issues': len([r for r in self.results.get('recommendations', []) 
                                  if r['priority'] == 'HIGH'])
        }
    
    def create_test_summary(self):
        """Create test category summary"""
        tests = self.results.get('tests', {})
        categories = defaultdict(lambda: {'total': 0, 'passed': 0, 'failed': 0})
        
        for test_name, test_data in tests.items():
            # Determine category from test name
            if test_name.startswith('0'):
                category = test_name.split('_', 1)[1].split('_')[0]
            else:
                category = 'other'
            
            categories[category]['total'] += 1
            if test_data['result'] in ['PASS', 'EXPECTED_FAIL']:
                categories[category]['passed'] += 1
            elif test_data['result'] == 'FAIL':
                categories[category]['failed'] += 1
        
        return dict(categories)
    
    def create_failure_details(self):
        """Create detailed failure analysis"""
        failures = {}
        
        for test_name, test_data in self.results.get('tests', {}).items():
            if test_data['result'] == 'FAIL':
                failures[test_name] = {
                    'errors': test_data['errors'],
                    'warnings': test_data['warnings'],
                    'key_lines': self.extract_key_lines(test_data['content'])
                }
        
        return failures
    
    def extract_key_lines(self, content):
        """Extract most relevant lines from log content"""
        lines = content.split('\n')
        key_lines = []
        
        for line in lines:
            if any(keyword in line.lower() for keyword in 
                  ['error', 'failed', 'timeout', 'segfault', 'libusb', 'sr:']):
                key_lines.append(line.strip())
        
        return key_lines[:10]  # Limit to most relevant lines

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 analyze_test_logs.py <log_directory>")
        sys.exit(1)
    
    log_dir = sys.argv[1]
    if not os.path.exists(log_dir):
        print(f"Error: Log directory '{log_dir}' not found")
        sys.exit(1)
    
    analyzer = TestLogAnalyzer(log_dir)
    report = analyzer.analyze_all_logs()
    
    # Print summary
    print("\n" + "="*60)
    print("LABJACK U12 TEST LOG ANALYSIS REPORT")
    print("="*60)
    
    summary = report['summary']
    print(f"\n📊 SUMMARY:")
    print(f"   Total Tests: {summary['total_tests']}")
    print(f"   Passed: {summary['passed']}")
    print(f"   Failed: {summary['failed']}")
    print(f"   Success Rate: {summary['success_rate']}")
    print(f"   Critical Issues: {summary['critical_issues']}")
    
    # Print recommendations
    if report['recommendations']:
        print(f"\n🚨 RECOMMENDATIONS:")
        for i, rec in enumerate(report['recommendations'], 1):
            print(f"   {i}. [{rec['priority']}] {rec['category']}")
            print(f"      Issue: {rec['issue']}")
            print(f"      Action: {rec['action']}")
    
    # Print error patterns
    if report['error_analysis']:
        print(f"\n🔍 TOP ERROR PATTERNS:")
        for pattern, count in list(report['error_analysis'].items())[:5]:
            print(f"   • {pattern} ({count} times)")
    
    # Save detailed report
    report_file = Path(log_dir) / "analysis_report.json"
    with open(report_file, 'w') as f:
        json.dump(report, f, indent=2)
    
    print(f"\n📄 Detailed report saved to: {report_file}")
    print("\n✅ Analysis complete!")

if __name__ == "__main__":
    main()
