using System;

namespace example {
    static class Program {
        private static int abcdValue() {
            return 123;
        }
        
        private static int abcd = 123;
        
        private static int test(string value) {
            return abcd;
        }
        
        static void Main() {
            var result = test("Test");
            Console.WriteLine(result);
        }
    }
    
}