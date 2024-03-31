# tema1

Tema 1 APD
Bogdan Elena-Catalina 331 CC

    Fisierul tema1_par.c contine paralelizarea implementarii algoritmului ce genereaza contururi ale unor harti topologice, folosind Marching Squares Algorithm.

    Functiile de baza ale acestei abordari sunt cele continute in scheletul
    initial, fiind ulterior modificate astfel incat sa fie permisa executia
    in paralel a algoritmului, fiecare thread ocupandu-se de anumite linii
    de pixeli ai imaginii input, determinate de valorile start si end.

    Pentru evitarea folosirii variabilelor globale a fost creata o structura
    "iteration" ce contine toate variabilele necesare unui thread, in functia
    main folosind un vector de structuri ce face posibila accesarea resurselor
    fara a declansa data races.

    Functiile rescale, sample_grid si march permit paralelizarea, asa ca se
    realizeaza o functie auxiliara alg pentru a fi pasata fiecarui thread creat.

    1. Rescale : deoarece nu se modifica decat imaginile care au dimensiuni 
    prea mari, vom trata acest caz in main, atribuind campului "scaled_image"
    chiar imaginea initiala in cazul in care are dimensiunile potrivite.
    Altfel, respectand algoritmul de redimensionare, se va actualiza cu noile valori calculate.

    2. Sample_grid : folosind imaginea scalata, dorim sa construim patratul de 
    grid al acesteia. De asemenea, se respecta algoritmul initial, iar memoria
    se aloca in functia main.

    3. March : updateaza conturul imaginii scalate

    In final, dupa asteptarea thread-urilor se scrie rezultatul in fisierul de
    output si se elibereaza memoria alocata.

    Mentionez folosirea ca materiale auxiliare : informatiile, exemplele si exercitiile
    incluse in laboratoarele 1, 2, 3 
    https://mobylab.docs.crescdi.pub.ro/docs/parallelAndDistributed/introduction
