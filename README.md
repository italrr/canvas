canvas
------

canvas is an highly expressive multi-paradigm scripting language. canvas is designed as the main scripting language for `nite` engine.



Some valid `canvas` examples:

```
def n -> 5;
def list -> [1, 2, 3, 4];

def sum -> (x, y){
    return + x y;
};

for(def i 0 in 1..20){
    if(eq (mod i 3) 0){
        print('fizz\n');
    }else
    if(eq (mod i 5) 0){
        print('buzz\n');
    }else
    if(eq (mod i 15) 0){
        printf('fizzbuzz\n');
    }
}

def Animal -> {
    def name;
    def legs;
    def sound -> (){
        return '';
    };
};

def cat -> splice Animal {
    def name -> 'cat';
    def legs -> 4;
    def sound -> (){
        return 'meowww!';
    };
};

```
